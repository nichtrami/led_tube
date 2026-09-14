#ifndef WIRELESSDMX_H
#define WIRELESSDMX_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "Dto.hpp"

constexpr uint8_t broadcast_address[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

class WirelessDmxController {
public:
    WirelessDmxController();
    WirelessDmxController(const WirelessDmxController&) = delete;
    WirelessDmxController& operator=(const WirelessDmxController&) = delete;
    WirelessDmxController(WirelessDmxController&&) = delete;
    WirelessDmxController& operator=(WirelessDmxController&&) = delete;

    void init(uint8_t channel);
    void sync_channel(uint8_t channel);
    void write(DmxFrame frame);
    TubeInput read(uint8_t address);
    void set_channel(uint8_t channel);

private:
    /** @brief Last accepted frame. size 0 means nothing has been received yet. */
    DmxFrame dmx_frame{};
    portMUX_TYPE dmx_frame_mux = portMUX_INITIALIZER_UNLOCKED;
    esp_now_peer_info_t peer_info{};
    uint8_t current_channel  = 0;
    uint8_t last_synced_channel = 255;
    static void on_data_recv(const uint8_t *mac_addr, const uint8_t *incoming_data, int len);
    static WirelessDmxController* instance;
};

#endif
