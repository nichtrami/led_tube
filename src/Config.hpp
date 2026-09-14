#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#include <Arduino.h>

#define DEVICE_MODE_TUBE 0
#define DEVICE_MODE_BRIDGE 1

/* Pin configuration */

#if DEVICE_MODE == DEVICE_MODE_TUBE
//LEDs
constexpr uint8_t LED_DATA_PIN = 4;

//I2C Display
constexpr uint8_t I2C_SCL = 2;
constexpr uint8_t I2C_SDA = 3;

//Menu Buttons
constexpr uint8_t PIN_UP = 7;
constexpr uint8_t PIN_DOWN = 9;
constexpr uint8_t PIN_ENTER = 8;


// /* pins for deprecated tube pcb v1 */
// constexpr uint8_t LED_DATA_PIN = 21;
// // constexpr uint8_t ENABLE_INTERNAL_LED_CONTROL_PIN = 5; //TODO: change to correct pins
// // constexpr uint8_t ENABLE_EXTERNAL_LED_CONTROL_PIN = 6;

// //I2C Display
// constexpr uint8_t I2C_SCL = 26;
// constexpr uint8_t I2C_SDA = 25;

// //Menu Buttons
// constexpr uint8_t PIN_UP = 33;
// constexpr uint8_t PIN_DOWN = 32;
// constexpr uint8_t PIN_ENTER = 27;

#elif DEVICE_MODE == DEVICE_MODE_BRIDGE
//LEDs
constexpr uint8_t LED_DATA_PIN = 14;

//I2C Display
constexpr uint8_t I2C_SCL = 25;
constexpr uint8_t I2C_SDA = 26;

//Menu Buttons
constexpr uint8_t PIN_UP = 27;
constexpr uint8_t PIN_DOWN = 32;
constexpr uint8_t PIN_ENTER = 33;

//Ethernet Module USR ES1 (W5500)
constexpr uint8_t W5500_RST = 4;
constexpr uint8_t SPI_CS = 5;                         
constexpr uint8_t SPI_SCK  = 18;
constexpr uint8_t SPI_MISO = 19;
constexpr uint8_t SPI_MOSI = 23;

//DMX
constexpr uint8_t RX_DMX = 16;
constexpr uint8_t TX_DMX = 17; //unneeded
constexpr uint8_t EN_DMX = 17; // Only RX is needed in project, so set EN to TX to save free pins

#endif

//I2C Display


// LED configuration
constexpr uint8_t NUM_LEDS = 120;

constexpr uint8_t OVERHEAD_DMX_FRAME = 2; // size + start_address bytes
constexpr uint8_t MAX_WDMX_CHANNELS = 240; //esp-now v1 is restricted to 250 byte
constexpr uint8_t MAX_WDMX_FRAME_SIZE = MAX_WDMX_CHANNELS + OVERHEAD_DMX_FRAME;

constexpr uint8_t TUBE_DATASET_SIZE = 4;

//delete this function after development is done
// void printCycleCount() {
//     static unsigned long lastMillis = 0;
//     static unsigned long cycleCount = 0;
//     cycleCount++;
//     unsigned long now = millis();
//     if(now - lastMillis >= 1000) {
//         Serial.print("Cycles pro Sekunde: ");
//         Serial.println(cycleCount);
//         cycleCount = 0;
//         lastMillis = now;
//     }
// }

#endif