#ifndef ESPNOW_TEST_HPP
#define ESPNOW_TEST_HPP
#pragma once

// Shared config + receiver-side radio init for the reliability test.
// The sender uses the real WirelessDmxController; the receiver only needs the
// radio set up on the same channel plus the optional RSSI sniffer.

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

// Keep these identical on bridge and tube.
constexpr uint8_t  TEST_LOGICAL_CHANNEL = 1;   // sender: WirelessDmx maps 1/2/3 -> WiFi 1/5/10
constexpr uint8_t  TEST_WIFI_CHANNEL    = 1;   // receiver: must match the sender's physical channel
constexpr uint16_t TEST_SEND_HZ         = 40;  // packets/s (~DMX refresh)
constexpr uint8_t  TEST_PAYLOAD_BYTES   = 80;  // DMX channels per frame (<= 240)

constexpr uint32_t TEST_SEND_INTERVAL_MS  = 1000UL / TEST_SEND_HZ;
constexpr uint32_t TEST_TX_INTERVAL_MS    = 5;    // how often a packet is actually transmitted
constexpr uint8_t  test_broadcast_address[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Last RSSI seen by the sniffer (function-local static -> single definition).
static inline volatile int& test_last_rssi() {
    static volatile int v = 0;
    return v;
}

static inline void test_sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    auto* p = static_cast<const wifi_promiscuous_pkt_t*>(buf);
    test_last_rssi() = p->rx_ctrl.rssi;
}

// STA + ESP-NOW on the fixed channel. The recv callback is registered by the caller.
static inline bool test_receiver_radio_init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    if (esp_now_init() != ESP_OK) {
        Serial.println("# ERROR esp_now_init failed");
        return false;
    }
    esp_wifi_set_channel(TEST_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

    // RSSI sniffer
    wifi_promiscuous_filter_t filt{};
    filt.filter_mask = WIFI_PROMIS_FILTER_MASK_DATA;
    esp_wifi_set_promiscuous_filter(&filt);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&test_sniffer_cb);
    esp_wifi_set_channel(TEST_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);  // re-set after promiscuous
    return true;
}

#endif // ESPNOW_TEST_HPP
