#include "DmxReceiver.hpp"

#if DEVICE_MODE == DEVICE_MODE_BRIDGE

const dmx_port_t DMX_NUM = DMX_NUM_1;
constexpr uint8_t DMX_TIMEOUT_MS = 100;

void DmxReceiver::begin() {
    dmx_config_t config = DMX_CONFIG_DEFAULT;
    dmx_driver_install(DMX_NUM, &config, NULL, 0);
    dmx_set_pin(DMX_NUM, TX_DMX, RX_DMX, EN_DMX);
    Serial.println("DMX Controller initialized");
}

/**
 * @brief Polls the DMX hardware and updates the internal data buffer.
 *
 * Checks for incoming DMX packets within a defined timeout. If new data is
 * available, it reads the full payload (up to 512 bytes) into the internal buffer. 
 */
void DmxReceiver::update() {
    dmx_packet_t packet;
    size_t received_size = dmx_receive(DMX_NUM, &packet, DMX_TIMEOUT_MS / portTICK_PERIOD_MS);

    if (received_size > 0) {
        dmx_read(DMX_NUM, dmx_buffer.data, received_size);
        dmx_buffer.size = static_cast<uint16_t>(received_size);
    } 
}

/**
 * @brief Extracts DMX data for a specific number of tubes starting at a given address.
 * 
 * Safely copies data from the internal DMX buffer into a new frame. Incorporates 
 * bounds checking to prevent buffer overflows. If the requested size exceeds the 
 * available data, only the remaining available bytes are copied.
 * 
 * @param start_address The DMX starting address (array index) to read from.
 * @param num_tubes The number of tubes, determining the requested payload size.
 * @return DmxFrame A frame containing the extracted DMX data, bounded by available capacity.
 */
DmxFrame DmxReceiver::read(uint16_t start_address, uint8_t num_tubes) {
    DmxFrame dmx_frame;

    if (start_address == 0 || start_address > dmx_buffer.size) { //dmx addresses are 1-based, so 0 is invalid
        Serial.println("Error: Invalid start_address for DMX read.");
        return dmx_frame;
    }

    size_t desired_size = static_cast<size_t>(num_tubes) * TUBE_DATASET_SIZE;
    size_t available_bytes = dmx_buffer.size - start_address;
    size_t bytes_to_copy = std::min({desired_size, available_bytes, static_cast<size_t>(MAX_WDMX_CHANNELS)});

    if (bytes_to_copy > 0) {
        memcpy(dmx_frame.data, &dmx_buffer.data[start_address], bytes_to_copy);
        dmx_frame.size = bytes_to_copy;
        dmx_frame.start_address = start_address;
    }

    // for(size_t i = 0; i < dmx_frame.size; i++){
    //     Serial.print(dmx_frame.data[i]);
    //     Serial.print(" ");
    // }
    // Serial.println();

    return dmx_frame;
}

#endif