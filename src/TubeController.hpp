#ifndef TUBECONTROLLER_H
#define TUBECONTROLLER_H

#include <Arduino.h>
#include <FastLED.h>
#include "SoundController.hpp"
#include "Dto.hpp"

#define MAX_TUBES 20

/** @brief wave()/pulse_wave() restart if they were not called for this many ms. */
const uint8_t RESET_DELAY = 100;

enum TubeEffect : uint8_t {
    TUBE_FILL_FAST                   = 0,  // forwards LED_PULSE_FAST to all tubes
    TUBE_BEAT_FLASH                  = 1,
    TUBE_FILL_FAST_2                 = 2,  // same as TUBE_FILL_FAST
    TUBE_WAVE                        = 3,
    TUBE_PULSE_WAVE                  = 4,
    TUBE_FLOW                        = 5,
    TUBE_REVERSE_FLOW                = 6,
    TUBE_FLOW_ALTERNATE              = 7,
    TUBE_STROBO                      = 8,
    TUBE_BEAT_HALF_SWAP              = 9,
    TUBE_DOUBLE_BEAT_HALF_SWAP       = 10,
    TUBE_BEAT_FLASH_ALTERNATE        = 11,
    TUBE_DOUBLE_BEAT_FLASH_ALTERNATE = 12,
    TUBE_TOGGLE_RED_BLUE             = 13,
    TUBE_TOGGLE_BLACK_PURPLE         = 14,
    TUBE_RANDOM_FLASH                = 15,
    TUBE_RAINBOW_ROW                 = 16,
    TUBE_ROW_COMET                   = 17,
    TUBE_CENTER_OUT                  = 18,
    TUBE_BUILD_UP                    = 19,
    /* 21..33: forward a single LedEffect to all tubes */
    TUBE_ALL_PULSE                   = 21,
    TUBE_ALL_PULSE_SLOW              = 22,
    TUBE_ALL_PULSE_FAST              = 23,
    TUBE_ALL_BOTTOM_TO_TOP           = 24,
    TUBE_ALL_TOP_TO_BOTTOM           = 25,
    TUBE_ALL_UP_AND_DOWN             = 26,
    /* 27..29: former bar effects, removed. IDs stay reserved so 30 and 31 keep their meaning. */
    TUBE_ALL_STROBO                  = 30,
    TUBE_ALL_RAINBOW                 = 31,
    TUBE_ALL_SELECTED                = 32
};

class Tube{
    public:
        uint8_t addr, effect;
        CRGB color;

        Tube() : Tube(0) {} 
        Tube(uint8_t addr);
        void set(uint8_t effect, CRGB color);
};

class TubeController
{
    public:
        TubeController();
        void handle();
        DmxFrame get_payload();

        void set_beat();
        void set_effect(uint8_t effect);
        void set_tube_effect(uint8_t effect);
        void set_color(CRGB color);
        void set_tubes_size(uint8_t size);
        void set_bpm(uint8_t bpm);

        void cycle_color();
        void cycle_effect();
        
    private:
        uint8_t effect;
        uint8_t tube_effect;
        CRGB color;
        bool beat_indicator;
        unsigned long beat_time;
        
        SoundController sound;

        Tube all_tubes[MAX_TUBES];
        uint8_t all_tubes_size = MAX_TUBES;
        uint8_t payload[MAX_TUBES * TUBE_DATASET_SIZE];

        /* helper */
        void tubes_off();
        void fill_solid(uint8_t effect, CRGB color);
        void generate_payload();
        uint8_t interpolate(uint8_t start, uint8_t end, uint8_t step, uint8_t steps);

        /**
         * @brief Index of the first tube of the second half.
         *
         * Derived on every call so the effects follow a tube count changed at
         * runtime via the menu. Must not be cached in a static.
         */
        uint8_t middle_index() const { return all_tubes_size / 2; }

        /** @brief From 8 tubes on, the random flash effects light two tubes instead of one. */
        bool use_second_tube() const { return all_tubes_size > 7; }

        /* effects */
        void pulse();
        void beat_flash();
        void beat_flash_with_strobo();
        void double_beat_flash();
        void beat_half_swap();
        void double_beat_half_swap();
        void beat_flash_alternate();
        void double_beat_flash_alternate();
        void flow();
        void reverse_flow();
        void flow_alternate();
        void wave();
        void pulse_wave();
        void random_flash();
        void rainbow_row();
        void row_comet();
        void center_out();
        void build_up();
        void sprinkles();
        void strobo();
        void solid_strobo( uint8_t speed );
        void toggle(CRGB color1, CRGB color2);
    };

    #endif