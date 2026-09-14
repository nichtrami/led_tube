#include "ArtNetReceiver.hpp"

#include <algorithm>

#if DEVICE_MODE == DEVICE_MODE_BRIDGE

uint8_t ArtNetReceiver::mac[] = { 0x02, 0x23, 0x45, 0x67, 0x89, 0xAB };

/** @brief Time budget for the whole DHCP handshake. */
static constexpr unsigned long DHCP_TIMEOUT_MS = 20;
/** @brief Time budget for a single DHCP response. */
static constexpr unsigned long DHCP_RESPONSE_TIMEOUT_MS = 1000;

ArtNetReceiver::ArtNetReceiver() {
    dmx_data_len = 0;
    new_data_available = false;
    initialized = false;
}

void ArtNetReceiver::begin() {
    reset_w5500();

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_CS);
    Ethernet.init(SPI_CS);

    Serial.println("DHCP started...");
    try{
        Ethernet.begin(mac, DHCP_TIMEOUT_MS, DHCP_RESPONSE_TIMEOUT_MS);
    } catch (...) {
        Serial.println("DHCP failed - ArtNet not available");
        initialized = false;
        return;
    }

    Serial.print("IP: ");
    Serial.println(Ethernet.localIP());

    artnet.begin();
    initialized = true;
    Serial.println("ArtNet initialized");
}

void ArtNetReceiver::reset_w5500() {
    pinMode(W5500_RST, OUTPUT);
    digitalWrite(W5500_RST, LOW);
    delay(100);
    digitalWrite(W5500_RST, HIGH);
    delay(200);
    Serial.println("W5500 reset complete");
}

void ArtNetReceiver::set_universe(uint8_t net, uint8_t sub_net, uint8_t universe) {
    if (net > 0x7F) {
            Serial.println(F("net should be less than 128"));
            return;
        }
        if (sub_net > 0xF) {
            Serial.println(F("sub_net should be less than 16"));
            return;
        }
        if (universe > 0xF) {
            Serial.println(F("universe should be less than 16"));
            return;
        }
    artnet.subscribeArtDmxUniverse(net, sub_net, universe, [this](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote) {
        uint16_t bytes_to_copy = (size > sizeof(dmx_data)) ? sizeof(dmx_data) : size;

        /* Runs in the ArtNet task on core 0 while read() runs in loop() on core 1, so buffer and length have to be published together. */
        portENTER_CRITICAL(&dmx_data_mux);
        memcpy(dmx_data, data, bytes_to_copy);
        dmx_data_len = bytes_to_copy;
        new_data_available = true;
        portEXIT_CRITICAL(&dmx_data_mux);
    });
}

void ArtNetReceiver::parse() {
    artnet.parse();
}

/**
 * @brief Copies the last received universe into a PixelControlInput.
 *
 * Belongs to the Pixel Controller Bridge Mode, which is currently replaced by
 * the ArtNet Bridge Mode in main.cpp. Kept so that mode can be restored.
 *
 * Only as many bytes as were actually received are taken over, capped at the
 * strip length; any remaining pixels stay black.
 *
 * @return PixelControlInput with `size` as a pixel count, as LedController expects.
 */
PixelControlInput ArtNetReceiver::read() {
    PixelControlInput input{};

    portENTER_CRITICAL(&dmx_data_mux);
    uint16_t bytes_to_copy = (dmx_data_len > sizeof(input.pixels)) ? sizeof(input.pixels) : dmx_data_len;
    memcpy(input.pixels, dmx_data, bytes_to_copy);
    new_data_available = false;
    portEXIT_CRITICAL(&dmx_data_mux);

    input.size = bytes_to_copy / sizeof(CRGB);
    return input;
}

/**
 * @brief Cuts the tubes' address range out of the last received universe.
 *
 * Mirrors DmxReceiver::read() so the ArtNet path produces the exact same frames
 * as the wired DMX path. Note the different index base: the esp_dmx buffer holds
 * the start code at index 0, so channel N sits at index N, while the ArtNet
 * payload is already channel-1-based and channel N sits at index N - 1.
 *
 * The bounds check and the copy share one critical section because the ArtNet
 * task on core 0 can replace the buffer between them, which would make the check
 * apply to a different universe than the copy. Serial output stays outside.
 *
 * @param start_address The DMX starting address (1-based) to read from.
 * @param num_tubes The number of tubes, determining the requested payload size.
 * @return DmxFrame A frame with the extracted slice, or size 0 if nothing is available.
 */
DmxFrame ArtNetReceiver::read_frame(uint16_t start_address, uint8_t num_tubes) {
    DmxFrame frame{};

    /* dmx addresses are 1-based, and DmxFrame::start_address only holds a byte */
    if (start_address == 0 || start_address > UINT8_MAX) {
        Serial.println("Error: Invalid start_address for ArtNet read.");
        return frame;
    }

    uint16_t offset = start_address - 1;
    size_t desired_size = static_cast<size_t>(num_tubes) * TUBE_DATASET_SIZE;

    portENTER_CRITICAL(&dmx_data_mux);
    if (offset < dmx_data_len) {
        size_t available_bytes = dmx_data_len - offset;
        size_t bytes_to_copy = std::min({desired_size, available_bytes, static_cast<size_t>(MAX_WDMX_CHANNELS)});

        memcpy(frame.data, &dmx_data[offset], bytes_to_copy);
        frame.size = bytes_to_copy;
        frame.start_address = static_cast<uint8_t>(start_address);
    }
    new_data_available = false;
    portEXIT_CRITICAL(&dmx_data_mux);

    return frame;
}

bool ArtNetReceiver::new_packet() {
    return new_data_available;
}

bool ArtNetReceiver::is_initialized() {
    return initialized;
}

void ArtNetReceiver::check_and_reinit() {
    if (!initialized && Ethernet.linkStatus() == LinkON) {
        Serial.println("Ethernet cable connected, trying to initialize ArtNet...");
        begin();
    }
}

#endif