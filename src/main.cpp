#include <Arduino.h>
#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "LedController.hpp"
#include "WirelessDmx.hpp"
#include "TubeController.hpp"
#include "Menu.hpp"
#include "Display.hpp"
#include "DmxReceiver.hpp"
#include "ArtNetReceiver.hpp"
#include "Config.hpp"


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Menu menu;
WirelessDmxController wireless_dmx;
LedController led_controller;

#if DEVICE_MODE == DEVICE_MODE_BRIDGE

DmxReceiver dmx;
TubeController tube_controller;
ArtNetReceiver artnet;

void artnet_task(void *pvParameters) {
    for (;;) {
        artnet.parse();
        vTaskDelay(2); 
    }
}
#endif

void setup() {
    Serial.begin(115200);
    menu.begin();
    led_controller.begin();
    wireless_dmx.init(menu.get_channel());

#if DEVICE_MODE == DEVICE_MODE_BRIDGE
    dmx.begin();

    Serial.println("Starting ArtNet Controller...");
    artnet.begin();
    artnet.set_universe(0, 0, 0);

    xTaskCreatePinnedToCore(
        artnet_task,             /*task function*/
        "ArtNetTask",            /*name of task*/
        10000,                   /*stack size of task*/
        NULL,                    /*parameter of the task*/
        tskIDLE_PRIORITY,                       /*priority of the task*/
        NULL,                    /*task handle to keep track of the created task*/
        0);                      /*core number to run the task*/
#endif
}

void loop() {
    menu.handle();
    wireless_dmx.sync_channel(menu.get_channel());

#if DEVICE_MODE == DEVICE_MODE_TUBE

    switch(menu.get_mode()) {
        case 0: { /* Standalone Mode */
            CRGB c = menu.get_color();
            led_controller.set_input(TubeInput{menu.get_effect(), c.r, c.g, c.b});
            break;
        }

        case 1: /* Wireless DMX Mode */
            led_controller.set_input(wireless_dmx.read(menu.get_address()));
            break;
            
        default:
            Serial.println("Error: Invalid Mode Selected");
    }

    led_controller.handle();

#elif DEVICE_MODE == DEVICE_MODE_BRIDGE

    switch(menu.get_mode()) {
        case 0: /* Master Bridge Mode */
            tube_controller.set_tubes_size(menu.get_tube_number());
            tube_controller.set_bpm(menu.get_bpm());
            menu.is_color_auto()  ? tube_controller.cycle_color()  : tube_controller.set_color(menu.get_color());
            menu.is_effect_auto() ? tube_controller.cycle_effect() : tube_controller.set_effect(menu.get_effect());
            tube_controller.set_tube_effect(menu.get_tube_effect());

            tube_controller.handle();
            wireless_dmx.write(tube_controller.get_payload());
            break;

        case 1: /* Wireless DMX Bridge Mode */
            dmx.update();
            wireless_dmx.write(dmx.read(menu.get_address(), menu.get_tube_number()));
            break;

        case 2: /* ArtNet Bridge Mode */
            artnet.check_and_reinit();
            if (artnet.new_packet()) {
                wireless_dmx.write(artnet.read_frame(menu.get_address(), menu.get_tube_number()));
            }
            break;

        // Replaced by the ArtNet Bridge Mode above. Kept for a possible rollback:
        // case 2: /* Pixel Controller Bridge Mode */
        //     artnet.check_and_reinit();
        //     if(artnet.new_packet()) {
        //         led_controller.set_input(artnet.read());
        //         led_controller.handle();
        //     }
        //     break;

        default:
            Serial.println("Error: Invalid Mode Selected");
    }

#endif

    //printCycleCount();
}
