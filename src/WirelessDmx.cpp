#include "WirelessDmx.hpp"

WirelessDmxController* WirelessDmxController::instance = nullptr;

/**
 * @brief Construct a new Wireless Dmx Controller object.
 * 
 * Initializes the singleton instance, sets the WiFi mode to STA,
 * initializes ESP-NOW, and registers the send and receive callbacks.
 */
WirelessDmxController::WirelessDmxController(){
    instance = this;
}

void WirelessDmxController::init(uint8_t channel){
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(); //to disable AP mode
    uint8_t menu_channel = (channel < 1 || channel > 3) ? 1 : channel;
    if(esp_now_init() != ESP_OK){
        Serial.println("Error: ESP-NOW initialization failed");
    }

    /* for sending */
    set_channel(menu_channel);
    last_synced_channel = menu_channel;

    /* for receiving */
    if (esp_now_register_recv_cb(on_data_recv) != ESP_OK) {
        Serial.println("Error: ESP-NOW receive callback registration failed");
    }

    Serial.println("ESP-NOW Initialized");
}

/**
 * @brief Synchronizes the wireless channel if it has changed.
 * @param channel Logical menu channel in range [1..3].
 * @note The channel is not the same as the DMX address. 
 * It is a WiFi channel that the ESP-NOW communication uses. 
 * Each logical menu channel corresponds to a specific WiFi channel
 * to allow multiple light systems to operate without interference.
 */
void WirelessDmxController::sync_channel(uint8_t channel){
    if (channel == last_synced_channel) {
        return;
    }

    set_channel(channel);
    last_synced_channel = channel;
}

/**
 * @brief Sets the ESP-NOW/WiFi channel based on a logical channel.
 * @param channel Logical menu channel in range [1..3].
 */
void WirelessDmxController::set_channel(uint8_t channel){
    if (channel < 1 || channel > 3) {
        Serial.println("Error: Invalid channel. Channel must be between 1 and 3.");
        return;
    }

    static const uint8_t channel_map[] = {1, 5, 10};
    current_channel = channel_map[channel - 1];
    esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);

    memcpy(peer_info.peer_addr, broadcast_address, 6);
    peer_info.channel = current_channel;
    peer_info.encrypt = false;

    esp_now_del_peer(peer_info.peer_addr);
    esp_now_add_peer(&peer_info);
}

/**
 * @brief Static callback function that is invoked when ESP-NOW data is received.
 *
 * Parses the wire format ([size][start_address][data...]) field by field and
 * validates it completely before anything is written to dmx_frame, so a frame
 * whose header does not match the packet can never reach read().
 *
 * Runs in the WiFi task, not in an interrupt.
 *
 * @param mac_addr The MAC address of the sender.
 * @param incoming_data A pointer to the received data.
 * @param len The length of the received data in bytes.
 */
void WirelessDmxController::on_data_recv(const uint8_t *mac_addr, const uint8_t *incoming_data, int len) {
    if (instance == nullptr) {
        Serial.println("Error: WirelessDmxController instance is not set.");
        return;
    }

    // Header contains 2 bytes: size + start_address
    if (len < (int)(OVERHEAD_DMX_FRAME) || len > (int)(MAX_WDMX_FRAME_SIZE)) {
        Serial.println("Error: Invalid received frame length.");
        return;
    }

    uint8_t size          = incoming_data[0];
    uint8_t start_address = incoming_data[1];

    if (size > MAX_WDMX_CHANNELS || size > (len - OVERHEAD_DMX_FRAME)) {
        Serial.println("Error: Invalid frame payload size.");
        return;
    }

    if (start_address == 0) { // dmx start address is never 0, so the frame is malformed
        Serial.println("Error: Invalid frame start_address.");
        return;
    }

    portENTER_CRITICAL(&instance->dmx_frame_mux);
    instance->dmx_frame.size          = size;
    instance->dmx_frame.start_address = start_address;
    memcpy(instance->dmx_frame.data, incoming_data + OVERHEAD_DMX_FRAME, size);
    portEXIT_CRITICAL(&instance->dmx_frame_mux);
}

/**
 * @brief Sends a DMX frame via ESP-NOW broadcast.
 * 
 * @param frame A pointer to the DmxFrame object to be sent.
 */
void WirelessDmxController::write(DmxFrame frame){
    if (frame.size > MAX_WDMX_CHANNELS) {
        Serial.println("Error: Frame size exceeds MAX_WDMX_CHANNELS.");
        return;
    }

    if (frame.start_address == 0) {
        Serial.println("Error: Frame start_address is not set");
        return;
    }

    esp_now_send(broadcast_address, (uint8_t *)&frame, frame.size + 2); // +2 for size + start_address byte
}

/**
 * @brief Reads TubeInput data from the DMX frame at a specific address.
 *
 * Takes a private snapshot of the frame under the lock and validates that,
 * instead of the shared member: the receive callback runs in the WiFi task and
 * could otherwise replace the frame between the bounds check and the copy, so
 * that the check would apply to a different frame than the copy. On the
 * snapshot, checks and copy are guaranteed to describe the same frame, and
 * Serial output stays outside the critical section.
 *
 * @param address The starting address within the DMX data array to read from.
 * @return TubeInput A struct containing the data read from the frame, or all
 *         zeros if the address is not covered by the current frame.
 */
TubeInput WirelessDmxController::read(uint8_t address){
    TubeInput input{};

    DmxFrame frame;
    portENTER_CRITICAL(&dmx_frame_mux);
    frame = dmx_frame;
    portEXIT_CRITICAL(&dmx_frame_mux);

    if (address == 0) {
        Serial.println("Error: Address must be >= 1.");
        return input;
    }

    if (frame.size == 0) {
        return input;
    }

    if (address < frame.start_address) {
        Serial.println("Error: Address is before frame start_address.");
        return input;
    }

    uint16_t relative_offset = static_cast<uint16_t>(address - frame.start_address);
    if (relative_offset + sizeof(TubeInput) > frame.size) {
        Serial.println("Error: Requested TubeInput exceeds frame payload.");
        return input;
    }

    memcpy(&input, &frame.data[relative_offset], sizeof(TubeInput));
    return input;
}


