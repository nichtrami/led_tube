#ifndef DTO_HPP
#define DTO_HPP

#include <Arduino.h>
#include <FastLED.h>
#include "Config.hpp"


/**
 * @brief Represents the input parameters for controlling a single LED tube's effect.
 * 
 * This struct is used as a Data Transfer Object (DTO) to bundle all necessary
 * parameters for setting an effect, a primary color, and a background color on a tube.
 */
struct TubeInput {
public:
    uint8_t effect, red, green, blue;
};

/**
 * @brief Represents the input for direct pixel-level control of an LED tube.
 * 
 * This struct allows for setting the color of each individual pixel in an LED tube
 * directly, bypassing predefined effects.
 */
struct PixelControlInput{
public:
    uint16_t size;
    CRGB pixels[NUM_LEDS];
};

/**
 * @brief Represents a data packet for wireless DMX transmission.
 * 
 * This struct defines the payload format for sending DMX data wirelessly.
 * It includes the data itself, its size, and a starting address to allow
 * for sending partial DMX universe updates efficiently.
 */
struct DmxFrame {
public:
    /// @brief The number of valid DMX channels in the 'data' array.
    uint8_t size;
    
    /// @brief The starting DMX address for the data in this frame.
    /// This allows sending only a relevant slice of the DMX universe. The first
    /// byte in the 'data' array corresponds to this address.
    uint8_t start_address; // Formerly start_byte
    
    /// @brief Buffer containing the raw DMX channel data.
    uint8_t data[MAX_WDMX_CHANNELS];
};

#endif