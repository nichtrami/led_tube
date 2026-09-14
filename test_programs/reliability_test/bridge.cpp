// Reliability test SENDER (bridge). Transmits DmxFrames via the real
// WirelessDmxController at a fixed rate; the payload is a running uint8_t counter
// (wraps at 255) so the receiver can detect losses from gaps in it.
// Runs indefinitely; recording time is bounded host-side by results/capture.sh.
//   Build/Flash:  pio run -e test_reliability_bridge -t upload

#include "EspNowTest.hpp"
#include "../../src/WirelessDmx.hpp"

static WirelessDmxController wireless;
static DmxFrame frame;
static uint32_t sent_total = 0;
static uint32_t next_tx    = 0;
static uint32_t next_seq   = 0;
static uint32_t last_log   = 0;
static uint8_t  current_seq = 0;

void setup() {
    Serial.begin(115200);
    delay(300);

    wireless.init(TEST_LOGICAL_CHANNEL);
    frame.size          = TEST_PAYLOAD_BYTES;
    frame.start_address = 1;               // write() requires start_address != 0

    Serial.println("# role=sender component=WirelessDmxController");
    Serial.print("# logical_channel="); Serial.print(TEST_LOGICAL_CHANNEL);
    Serial.print(" rate_hz=");           Serial.print(TEST_SEND_HZ);
    Serial.print(" channels=");          Serial.println(TEST_PAYLOAD_BYTES);

    next_tx = next_seq = last_log = millis();
}

void loop() {
    uint32_t now = millis();

    // Advance sequence counter every 25 ms
    if ((int32_t)(now - next_seq) >= 0) {
        next_seq += TEST_SEND_INTERVAL_MS;
        if ((int32_t)(now - next_seq) > (int32_t)(TEST_SEND_INTERVAL_MS * 4))
            next_seq = now + TEST_SEND_INTERVAL_MS;
        current_seq++;                                   // wraps at 255
        for (uint8_t i = 0; i < frame.size; i++) frame.data[i] = current_seq;
    }

    // Transmit every 5 ms
    if ((int32_t)(now - next_tx) >= 0) {
        next_tx += TEST_TX_INTERVAL_MS;
        if ((int32_t)(now - next_tx) > (int32_t)(TEST_TX_INTERVAL_MS * 4))
            next_tx = now + TEST_TX_INTERVAL_MS;
        wireless.write(frame);
        sent_total++;
    }

    if (now - last_log >= 5000) {                        // heartbeat
        last_log = now;
        Serial.print("# sent_total="); Serial.println(sent_total);
    }
}
