#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include <FastLED.h>
#include "Dto.hpp"
#include "Config.hpp"

enum LedEffect : uint8_t {
    LED_SOLID              = 0,
    LED_PULSE              = 1,
    LED_PULSE_SLOW         = 2,
    LED_PULSE_FAST         = 3,
    LED_BOTTOM_TO_TOP      = 4,
    LED_TOP_TO_BOTTOM      = 5,
    LED_UP_AND_DOWN        = 6,
    LED_STROBO             = 10,

    LED_RAINBOW            = 100, 
    LED_RAINBOW_FLOW_SLOW  = 101,
    LED_RAINBOW_FLOW       = 102,
    LED_RAINBOW_FLOW_FAST  = 103,

    LED_COMET_SLOW         = 110,
    LED_COMET              = 111,
    LED_COMET_FAST         = 112,

    LED_SCANNER_SLOW       = 120,
    LED_SCANNER            = 121,
    LED_SCANNER_FAST       = 122,

    LED_CENTER_OUT_SLOW    = 130,
    LED_CENTER_OUT         = 131,
    LED_CENTER_OUT_FAST    = 132,

    LED_FIRE_CALM          = 140,
    LED_FIRE               = 141,
    LED_FIRE_WILD          = 142,

    LED_PLASMA_SLOW        = 150,
    LED_PLASMA             = 151,
    LED_PLASMA_FAST        = 152,

    LED_BOUNCE_SLOW        = 160,
    LED_BOUNCE             = 161,
    LED_BOUNCE_FAST        = 162,

    LED_WIPE_SLOW          = 170,
    LED_WIPE               = 171,
    LED_WIPE_FAST          = 172,

    LED_GRADIENT           = 180,
    LED_GRADIENT_CENTER    = 181,

    LED_SPARKLE_THIN       = 190,
    LED_SPARKLE            = 191,
    LED_SPARKLE_DENSE      = 192,

    LED_LIGHTNING_RARE     = 200, 
    LED_LIGHTNING          = 201,
    LED_LIGHTNING_WILD     = 202,

    LED_CHASE_SLOW         = 210,
    LED_CHASE              = 211,
    LED_CHASE_FAST         = 212
};

class LedController
{
    public:
        LedController() = default;
        void begin();

        void set_input(CRGB color);
        void set_input(TubeInput input);
        void set_input(PixelControlInput input);

        void handle();

    private:
        uint8_t effect = LED_SOLID;
        CRGB color = CRGB::Black;
        CRGB leds[NUM_LEDS] = {};

        uint8_t heat[NUM_LEDS] = {};

        unsigned long last_frame_ms = 0;
        unsigned long effect_start_ms = 0;

        void set_effect(uint8_t new_effect);
        void hsv2rgb_rainbow(uint8_t hue, uint8_t sat, uint8_t val, uint8_t &red, uint8_t &green, uint8_t &blue);
        uint8_t exponential_increase(uint8_t x);
        void fill_solid(struct CRGB * target_array, int num_to_fill, const struct CRGB& color);

        /* Drawing primitives shared by the effects. */
        void clear();        
        CRGB scaled_color(uint8_t brightness);
        void draw_block(int16_t position);
        int16_t sweep_position(uint32_t phase, uint16_t period_ms);
        void draw_tail(int16_t head, uint8_t length, int8_t direction);
        CRGBPalette16 fire_palette();

    
        void solid_color_mode();
        void pulse(uint32_t elapsed, uint16_t period_ms);
        void bottom_to_top(uint32_t elapsed);
        void top_to_bottom(uint32_t elapsed);
        void up_and_down(uint32_t elapsed);
        void strobo(uint32_t elapsed);
        void rainbow_solid(uint32_t elapsed);
        void rainbow_flow(uint32_t elapsed, uint16_t period_ms);
        void comet(uint32_t elapsed, uint16_t period_ms);
        void scanner(uint32_t elapsed, uint16_t period_ms);
        void center_out(uint32_t elapsed, uint16_t period_ms);
        void fire(uint8_t cooling, uint8_t sparking);
        void plasma(uint32_t elapsed, uint8_t speed);
        void bounce(uint32_t elapsed, uint16_t period_ms);
        void wipe(uint32_t elapsed, uint16_t period_ms);
        void gradient(bool from_center);
        void sparkle(uint8_t chance);
        void lightning(uint32_t elapsed, uint16_t gap_ms);
        void chase(uint32_t elapsed, uint16_t period_ms);
};

#endif
