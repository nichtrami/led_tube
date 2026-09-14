#ifndef ARTNET_RECEIVER_HPP
#define ARTNET_RECEIVER_HPP

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArtnetEther.h>
#include "Config.hpp"
#include "Dto.hpp"



class ArtNetReceiver {
    private:
        ArtnetEtherReceiver artnet;
        static uint8_t mac[];
        uint8_t net;
        uint8_t sub_net;
        uint16_t universe;
        uint8_t dmx_data[512];
        /** @brief Valid bytes in dmx_data. Written by the ArtNet task, read by loop(). */
        uint16_t dmx_data_len;
        /** @brief Guards dmx_data / dmx_data_len against the ArtNet task on the other core. */
        portMUX_TYPE dmx_data_mux = portMUX_INITIALIZER_UNLOCKED;
        volatile bool new_data_available;
        bool initialized;
    public:
        ArtNetReceiver();
        void begin();
        void reset_w5500();
        void set_universe(uint8_t net, uint8_t sub_net, uint8_t universe);
        void parse();
        bool new_packet();
        bool is_initialized();
        void check_and_reinit();
        /** @brief Pixel Controller Bridge Mode: universe as a raw RGB pixel buffer for the local strip. */
        PixelControlInput read();
        /** @brief ArtNet Bridge Mode: universe as a DMX slice for the ESP-NOW broadcast. */
        DmxFrame read_frame(uint16_t start_address, uint8_t num_tubes);
};

#endif