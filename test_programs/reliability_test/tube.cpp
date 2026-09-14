// Reliability test RECEIVER (tube). Receives the sender's DmxFrames and derives,
// per actually received packet (in the recv callback -> exact), the packet loss
// from gaps in the payload counter, plus inter-seq jitter and RSSI.
// Prints one CSV row per second; runs indefinitely (recording bounded by capture.sh).
//   Build/Flash:  pio run -e test_reliability_tube -t upload

#include "EspNowTest.hpp"
#include "../../src/Dto.hpp"   // DmxFrame
#include <math.h>

constexpr uint32_t REPORT_MS = 1000;

// Shared between recv callback and loop(); guarded by the spinlock.
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static bool     have_first  = false;
static uint8_t  last_seq    = 0;
static uint64_t recv_all    = 0;   // every physical packet incl. duplicates
static uint64_t recv_unique = 0;   // unique seq values first-seen
static uint64_t lost_count  = 0;
static uint32_t win_gap_max = 0;   // longest loss burst in the window
static uint32_t cum_gap_max = 0;   // longest loss burst overall

// Inter-seq IAT accumulators (only updated on new seq arrival)
static uint32_t prev_seq_arrival = 0;
static double   iat_sum          = 0.0;
static double   iat_sqsum        = 0.0;
static uint32_t iat_n            = 0;

static long     rssi_sum  = 0;     // RSSI accumulators (per window)
static uint32_t rssi_n    = 0;
static int      rssi_min  = 100;   // dBm < 0, so 100 = none yet

static void on_recv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len < (int)(OVERHEAD_DMX_FRAME + 1)) return;
    uint8_t seq = data[OVERHEAD_DMX_FRAME];   // first payload byte = counter
    uint32_t now = millis();

    portENTER_CRITICAL(&mux);
    recv_all++;

    if (!have_first) {
        have_first           = true;
        last_seq             = seq;
        recv_unique++;
        prev_seq_arrival     = now;
    } else {
        uint8_t advance = (uint8_t)(seq - last_seq);  // mod-256, handles wrap-around
        if (advance != 0) {
            uint32_t gap = advance - 1u;               // unique seq values lost
            lost_count += gap;
            if (gap > win_gap_max) win_gap_max = gap;
            if (gap > cum_gap_max) cum_gap_max = gap;
            last_seq = seq;
            recv_unique++;

            // Inter-seq IAT: only count time between new seq arrivals
            if (prev_seq_arrival != 0) {
                double d = (double)(now - prev_seq_arrival);
                iat_sum += d; iat_sqsum += d * d; iat_n++;
            }
            prev_seq_arrival = now;
        }
        // advance == 0: duplicate transmission, counted in recv_all only
    }

    int r = test_last_rssi();
    if (r != 0) {
        rssi_sum += r; rssi_n++;
        if (r < rssi_min) rssi_min = r;
    }
    portEXIT_CRITICAL(&mux);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (!test_receiver_radio_init()) {
        Serial.println("# FATAL radio init failed - halting");
        while (true) { delay(1000); }
    }
    esp_now_register_recv_cb(on_recv);

    Serial.println("# role=receiver");
    Serial.print("# channel=");  Serial.print(TEST_WIFI_CHANNEL);
    Serial.print(" expect_hz="); Serial.println(TEST_SEND_HZ);
    Serial.println("t_ms,win_recv_all,win_recv_unique,win_exp,win_lost,win_loss_pct,win_max_gap,"
                   "cum_recv_all,cum_recv_unique,cum_exp,cum_lost,cum_loss_pct,cum_max_gap,"
                   "recv_per_seq_mean,iat_seq_mean_ms,jitter_seq_ms,rssi_avg,rssi_min");
}

void loop() {
    static uint32_t next_report      = REPORT_MS;
    static uint64_t prev_recv_all    = 0;
    static uint64_t prev_recv_unique = 0;
    static uint64_t prev_lost        = 0;

    uint32_t now = millis();
    if ((int32_t)(now - next_report) < 0) return;
    next_report += REPORT_MS;
    if ((int32_t)(now - next_report) > 0) next_report = now + REPORT_MS;

    // Snapshot cumulative state, reset per-window accumulators.
    portENTER_CRITICAL(&mux);
    bool     hf   = have_first;
    uint64_t ra   = recv_all;
    uint64_t ru   = recv_unique;
    uint64_t lc   = lost_count;
    double   isum = iat_sum;
    double   isq  = iat_sqsum;
    uint32_t in   = iat_n;
    long     rsum = rssi_sum;
    uint32_t rn   = rssi_n;
    int      rmin = rssi_min;
    uint32_t wgap = win_gap_max;
    uint32_t cgap = cum_gap_max;
    iat_sum = 0.0; iat_sqsum = 0.0; iat_n = 0;
    rssi_sum = 0;  rssi_n = 0;      rssi_min = 100;
    win_gap_max = 0;
    portEXIT_CRITICAL(&mux);

    if (!hf) {   // link not up yet
        Serial.print(now);
        Serial.println(",0,0,0,0,100.00,0,0,0,0,0,100.00,0,0.00,0.00,0.00,,");
        return;
    }

    uint32_t win_recv_all    = (uint32_t)(ra - prev_recv_all);
    uint32_t win_recv_unique = (uint32_t)(ru - prev_recv_unique);
    uint32_t win_lost        = (uint32_t)(lc - prev_lost);
    uint32_t win_exp         = win_recv_unique + win_lost;
    double   win_loss        = (win_exp > 0) ? (100.0 * win_lost / win_exp) : 0.0;
    double   recv_per_seq    = (win_recv_unique > 0) ? ((double)win_recv_all / win_recv_unique) : 0.0;

    uint32_t cum_recv_all    = (uint32_t)ra;
    uint32_t cum_recv_unique = (uint32_t)ru;
    uint32_t cum_lost        = (uint32_t)lc;
    uint32_t cum_exp         = cum_recv_unique + cum_lost;
    double   cum_loss        = (cum_exp > 0) ? (100.0 * cum_lost / cum_exp) : 0.0;

    // jitter = std-dev of inter-seq inter-arrival time
    double iat_mean = (in > 0) ? (isum / in) : 0.0;
    double var      = (in > 0) ? (isq / in - iat_mean * iat_mean) : 0.0;
    double jitter   = (var > 0) ? sqrt(var) : 0.0;

    Serial.print(now);               Serial.print(',');
    Serial.print(win_recv_all);      Serial.print(',');
    Serial.print(win_recv_unique);   Serial.print(',');
    Serial.print(win_exp);           Serial.print(',');
    Serial.print(win_lost);          Serial.print(',');
    Serial.print(win_loss, 2);       Serial.print(',');
    Serial.print(wgap);              Serial.print(',');
    Serial.print(cum_recv_all);      Serial.print(',');
    Serial.print(cum_recv_unique);   Serial.print(',');
    Serial.print(cum_exp);           Serial.print(',');
    Serial.print(cum_lost);          Serial.print(',');
    Serial.print(cum_loss, 2);       Serial.print(',');
    Serial.print(cgap);              Serial.print(',');
    Serial.print(recv_per_seq, 2);   Serial.print(',');
    Serial.print(iat_mean, 2);       Serial.print(',');
    Serial.print(jitter, 2);         Serial.print(',');
    if (rn > 0) { Serial.print((double)rsum / rn, 1); Serial.print(','); Serial.print(rmin); }
    else        { Serial.print(','); }
    Serial.println();

    prev_recv_all    = ra;
    prev_recv_unique = ru;
    prev_lost        = lc;
}
