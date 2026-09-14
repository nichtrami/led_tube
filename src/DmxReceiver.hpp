#ifndef DMX_RECEIVER_H
#define DMX_RECEIVER_H

#include <Arduino.h>
#include <esp_dmx.h>
#include "Menu.hpp"
#include "Dto.hpp"
#include "Config.hpp"


#define DMX_SIZE DATASET_SIZE + 1 // +1 for bpm in transceiver mode

extern const dmx_port_t DMX_NUM;
extern uint8_t dmx_data[DMX_PACKET_SIZE];

class DmxReceiver {
public:
    DmxReceiver() = default;

    /** @brief Installs the DMX driver and assigns its pins. Call from setup(). */
    void begin();

    void update();
    DmxFrame read(uint16_t start_address, uint8_t num_tubes);

private:
    struct {
        uint16_t size;
        uint8_t data[DMX_PACKET_SIZE_MAX];
    } dmx_buffer{};
};

#endif