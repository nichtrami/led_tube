#include "TubeController.hpp"
#include "LedController.hpp"

/**
 * @brief Construct a new Tube object.
 *
 * This constructor initializes a new Tube object with the given address. 
 * The effect, red, green, blue and white properties are all set to 0.
 *
 * @param addr - The address to set for the Tube.
 */
Tube::Tube(uint8_t addr) {
  this->addr = addr;
  effect = 0;
}

/**
 * @brief Set the properties of the Tube.
 *
 * This function sets the effect, red, green, blue, and white properties of the Tubes. 
 *
 * @param effect - The (LedController-) effect to set for the Tube.
 * @param color - The color struct CRGB to set for the Tube.
 */
void Tube::set(uint8_t effect, CRGB color) {
    this->effect = effect;
    this->color = color;
}

TubeController::TubeController() {
    effect = TUBE_RANDOM_FLASH;
    tube_effect = LED_PULSE_FAST;

    sound = SoundController();
    beat_indicator = false;
    beat_time = millis();

    all_tubes_size = MAX_TUBES;
    for (uint8_t i = 0; i < all_tubes_size; i++) {
        all_tubes[i] = Tube(i);
    }
}


uint8_t TubeController::interpolate(uint8_t start, uint8_t end, uint8_t step, uint8_t steps) {
    return (start * (steps - step) + end * step) / steps;
}

/**
 * @brief Sets the size of the tubes array.
 *
 * This method sets the size of the `all_tubes` array to the specified value.
 * The size determines how many tubes are managed by the `TubeController`.
 * Values above MAX_TUBES are clamped, so a wider menu range can never write
 * past `all_tubes` / `payload`.
 *
 * @param size The number of tubes to set.
 */
void TubeController::set_tubes_size(uint8_t size) {
    all_tubes_size = (size > MAX_TUBES) ? MAX_TUBES : size;
}

/**
 * @brief Sets the beat indicator and beat time for the tube controller based on a predefined BPM.
 * 
 * This function configures the tube controller's beat indicator and beat time by utilizing the sound
 * object's capabilities to derive beat information from a fixed BPM value (125 BPM in this case).
 * The beat indicator is set based on the beat derived from the BPM, and the beat time is obtained
 * directly from the sound object, reflecting the current beat's timing characteristics.
 */
void TubeController::set_beat() {
   beat_indicator = sound.beat_from_bpm();
   beat_time = sound.get_beat_time();
}

void TubeController::set_effect(uint8_t effect) {
    this->effect = effect;
}

/**
 * @brief Sets the per-tube effect forwarded by TUBE_ALL_SELECTED.
 *
 * @param effect A LedEffect ID, taken from the bridge menu's "Tube FX" item.
 */
void TubeController::set_tube_effect(uint8_t effect) {
    this->tube_effect = effect;
}

/**
 * @brief Sets the beat tempo used to derive beats from BPM.
 *
 * Forwards the given BPM to the sound controller, which generates the
 * beat indicator and beat timing consumed by the beat-based effects.
 *
 * @param bpm The beats per minute (60-200) to drive the effects.
 */
void TubeController::set_bpm(uint8_t bpm) {
    sound.set_bpm(bpm);
}

void TubeController::set_color(CRGB color) {
    this->color = color;
}

/**
 * @brief Generates a payload from the global and tube data.
 *
 * This function first assigns the global data (rgb_mode, bg_color, bg_saturation, bg_brightness) to the first four elements of the payload array.
 * Then, it iterates over all tubes and for each tube, it assigns the tube's effect, hue, saturation, and value to the payload array.
 * The payload array is structured such that the first four elements represent the global data, and for each tube, the next four consecutive elements represent the tube's effect, hue, saturation, and value respectively.
 *
 * @return void
 */
void TubeController::generate_payload() {
    for (uint8_t i = 0; i < all_tubes_size; i++) {
        payload[i * TUBE_DATASET_SIZE] = all_tubes[i].effect;
        payload[i * TUBE_DATASET_SIZE + 1] = all_tubes[i].color.r;
        payload[i * TUBE_DATASET_SIZE + 2] = all_tubes[i].color.g;
        payload[i * TUBE_DATASET_SIZE + 3] = all_tubes[i].color.b;
    }
}

/**
 * @brief Constructs and returns a WirelessDmxFrame from the generated payload.
 * 
 * This function creates a WirelessDmxFrame struct, sets its size, copies the
 * internal payload buffer into the frame's data array, and returns the complete frame.
 * This is safer than returning a raw pointer.
 *
 * @return WirelessDmxFrame The fully constructed DMX frame ready for transmission.
 */
DmxFrame TubeController::get_payload() {
    DmxFrame frame{};
    uint8_t current_payload_size = all_tubes_size * TUBE_DATASET_SIZE;

    size_t bytes_to_copy = (current_payload_size > MAX_WDMX_CHANNELS) ? MAX_WDMX_CHANNELS : current_payload_size;
    memcpy(frame.data, payload, bytes_to_copy);

    frame.size = bytes_to_copy;
    frame.start_address = 1;
    return frame;
}

void TubeController::tubes_off() {
    for (int i = 0; i < all_tubes_size; i++) {
        all_tubes[i].set(0, CRGB::Black);
    }
}

/**
 * @brief Set all tubes to the same parameters.
 *
 * This function sets all tubes to the same effect, color, saturation and brightness.
 * 
 * @param effect The effect to be applied to all tubes. This should be a valid effect identifier.
 * @param color The color to be applied to all tubes. This should be a valid color identifier.
 * @param saturation The saturation level to be applied to all tubes. This should be a value between 0 (no saturation) and 1 (full saturation).
 * @param brightness The brightness level to be applied to all tubes. This should be a value between 0 (no brightness) and 1 (full brightness).
 *
 * @return void
 */
void TubeController::fill_solid(uint8_t effect, CRGB color) {
    for (int i = 0; i < all_tubes_size; i++) {
        all_tubes[i].set(effect, color);
    }
}

void TubeController::pulse() {
    for(int i = 0; i < all_tubes_size; i++){
        all_tubes[i].set(1, color);
    }
}


void TubeController::beat_flash() {
    static uint8_t selected_tube = 0;
    static uint8_t selected_tube_2 = 0;
    unsigned long fade_duration = (60000UL / max((uint8_t)1, sound.get_bpm())) / 3; // one third of the beat duration

    if (beat_indicator == 1) {
        selected_tube = random(all_tubes_size);
        selected_tube_2 = random(all_tubes_size);
    }

    unsigned long time_since_beat = millis() - beat_time;
    if (time_since_beat < fade_duration) {
        uint8_t tmp_red   = color.r * (fade_duration - time_since_beat) / fade_duration;
        uint8_t tmp_green = color.g * (fade_duration - time_since_beat) / fade_duration;
        uint8_t tmp_blue  = color.b * (fade_duration - time_since_beat) / fade_duration;

        all_tubes[selected_tube].set(0, CRGB(tmp_red, tmp_green, tmp_blue));
        if (use_second_tube()) all_tubes[selected_tube_2].set(0, CRGB(tmp_red, tmp_green, tmp_blue));
    }
}

void TubeController::beat_flash_with_strobo() {
    sprinkles();
    beat_flash();
}

/**
 * @brief Triggers a double flash effect on a random tube in response to a beat.
 * 
 * On beat, selects a random tube. Then, based on the elapsed time since the beat,
 * it alternates the tube's brightness between full and off to create a double flash effect within 140ms.
 */
void TubeController::double_beat_flash(){
    static uint8_t selected_tube = 0;

    beat_indicator == 1 ? selected_tube = random(all_tubes_size) : selected_tube;
    unsigned long time_since_beat = millis() - beat_time;

    if(time_since_beat < 20)       {all_tubes[selected_tube].set(0, color);}
    else if(time_since_beat < 100) {all_tubes[selected_tube].set(0, CRGB::Black);}
    else if(time_since_beat < 120) {all_tubes[selected_tube].set(0, color);}
    else if(time_since_beat < 140) {all_tubes[selected_tube].set(0, CRGB::Black);}
}

/**
 * @brief Toggles the brightness of the tubes between two halves around a middle point based on the beat indicator.
 * 
 * This function divides the tubes into two halves. On every beat (when beat_indicator equals 1),
 * it swaps the brightness between the two halves. The first half's brightness is set to the current
 * brightness value, and the second half's brightness is set to the background brightness value.
 * Between beats, it gradually decreases the brightness of the first half back to the background
 * brightness level.
 */
void TubeController::beat_half_swap() {
    static bool first_half = true;
    static const uint8_t num_steps = 5;
    static uint8_t step = num_steps; // 5 = inactiv at start

    if(beat_indicator == 1) {
        first_half = !first_half;
        step = 0;
    }
    if(step <= num_steps) {
        uint8_t tmp_red = interpolate(color.r, 0, step, num_steps);
        uint8_t tmp_green = interpolate(color.g, 0, step, num_steps);
        uint8_t tmp_blue = interpolate(color.b, 0, step, num_steps);
        step++;

        for (int i = 0; i < all_tubes_size; i++) {
            if ((i < middle_index()) == first_half) {
            all_tubes[i].set(0, CRGB(tmp_red, tmp_green, tmp_blue));
            } else {
            all_tubes[i].set(0, CRGB::Black);
            }
        }
    }
}

/**
 * @brief Toggles a double flash effect on half of the tube in sync with beats.
 * 
 * Alternates flashing between the two halves of tubes on each beat. Flashes occur within specific time intervals
 * after a beat: immediately for 20ms, off until 100ms, on again for 20ms, then off. The active half alternates with each beat.
 */
void TubeController::double_beat_half_swap(){
    static bool first_half = true;

    if (beat_indicator) first_half = !first_half;
    unsigned long time_since_beat = millis() - beat_time;

    bool tubes_on = false;
    if(time_since_beat < 20)       {tubes_on = true;}
    else if(time_since_beat < 100) {tubes_on = false;}
    else if(time_since_beat < 120) {tubes_on = true;}
    else if(time_since_beat < 140) {tubes_on = false;}

    if(tubes_on) {
        for (int i = 0; i < all_tubes_size; i++) {
            if ((i < middle_index()) == first_half) {
                all_tubes[i].set(0, color);
            } else {
                all_tubes[i].set(0, CRGB::Black);
            }
        }
    }
}

/**
 * @brief Alternates flashing between even and odd tubes in sync with beats.
 * 
 * This function is designed to create a visual effect in response to music or beats. On each beat, it toggles
 * between lighting up even and odd tubes. The brightness of the active tubes is set to `brightness`, while the
 * inactive tubes revert to `bg_brightness`. Between beats, the brightness of the active tubes gradually decreases
 * back to `bg_brightness` to create a fading effect.
 */
void TubeController::beat_flash_alternate() {
    static bool even_tubes = true;
    static const uint8_t num_steps = 5;
    static uint8_t step = num_steps;

    if (beat_indicator) {
        even_tubes = !even_tubes;
        step = 0;
    }
    if(step <= num_steps) {
        uint8_t tmp_red = interpolate(color.r, 0, step, num_steps);
        uint8_t tmp_green = interpolate(color.g, 0, step, num_steps);
        uint8_t tmp_blue = interpolate(color.b, 0, step, num_steps);
        step++;

        for (int i = 0; i < all_tubes_size; i++) {
            if (even_tubes ? (i % 2 == 0) : (i % 2 != 0)) {
                all_tubes[i].set(0, CRGB(tmp_red, tmp_green, tmp_blue));
            } else {
                all_tubes[i].set(0, CRGB::Black);
            }
        }
    }
}

/**
 * @brief Alternates flashing between even and odd tubes in response to beats, with a double flash effect.
 * 
 * This function creates a visual effect by alternating flashes between even and odd tubes in sync with music beats.
 * It uses a double flash pattern within a short timeframe after detecting a beat. The pattern is as follows:
 * - Within 20ms of the beat, the selected tubes (even or odd) are turned on.
 * - From 20ms to 100ms, all tubes are turned off.
 * - From 100ms to 120ms, the selected tubes are turned on again.
 * - After 120ms, all tubes are turned off.
 * The selection between even and odd tubes alternates with each beat detected.
 */
void TubeController::double_beat_flash_alternate() {
    static bool even_tubes = true;

    beat_indicator ? even_tubes = !even_tubes : even_tubes;
    unsigned long time_since_beat = millis() - beat_time;

    bool tubes_on = false;
    if(time_since_beat < 20)       {tubes_on = true;}
    else if(time_since_beat < 100) {tubes_on = false;}
    else if(time_since_beat < 120) {tubes_on = true;}
    else if(time_since_beat < 140) {tubes_on = false;}

    if(tubes_on) {
        for (int i = 0; i < all_tubes_size; i++) {
        if (even_tubes ? (i % 2 == 0) : (i % 2 != 0)) {
            all_tubes[i].set(0, color);
        } else {
            all_tubes[i].set(0, CRGB::Black);
        }
    }
    }else{
        tubes_off();
    }
}

//untested
void TubeController::sprinkles() {
    static const unsigned long SPRINKLE_INTERVAL = 5; // ms per step
    static const uint8_t FLASH_EVERY = 25;            // light up every Nth step
    static unsigned long last_change = 0;
    static uint8_t step = 0;

    if (millis() - last_change < SPRINKLE_INTERVAL) return; // non-blocking wait
    last_change = millis();

    uint8_t selected_tube = random(all_tubes_size);
    if (step >= FLASH_EVERY) {
        all_tubes[selected_tube].set(0, color);
        step = 0;
    } else {
        all_tubes[selected_tube].set(0, CRGB::Black);
        step++;
    }
}

/**
 * @brief Executes a strobe effect on a randomly selected tube.
 *
 * This function turns off all tubes and then randomly selects one tube to flash with a specified color, saturation, and brightness.
 * The strobe effect is controlled by a delay counter (`strobo_delay`), which determines when the selected tube will flash.
 * The function uses a static delay (implemented with `delay(5)`), which needs to be replaced by a non-blocking timer for better performance.
 * @note Uses non-blocking millis()-based timing so the main loop keeps running.
 */
void TubeController::strobo(){
    static const unsigned long STROBE_INTERVAL = 5; // ms per on/off phase
    static uint8_t selected_tube = 0;
    static uint8_t selected_tube_2 = 0;
    static bool strobo_on = false;
    static unsigned long last_change = 0;

    if (millis() - last_change < STROBE_INTERVAL) return; // hold current frame
    last_change = millis();

    tubes_off(); //set_backgroundcolor(); => if backgroundcolor is wished to be used during strobo
    strobo_on = !strobo_on;
    if (strobo_on) {
        selected_tube = random(all_tubes_size);
        selected_tube_2 = random(all_tubes_size);
        all_tubes[selected_tube].set(0, color);
        if (use_second_tube()) all_tubes[selected_tube_2].set(0, color);
    }
}

void TubeController::solid_strobo( uint8_t delay) {
    static bool strobo_state = false;
    static unsigned long change_time = 0;
 
    /* invert strobo_state */
    if( millis() - change_time > delay) {
        strobo_state = !strobo_state;
        change_time = millis();
    }

    /* set up lights */
    if(strobo_state) {
        for( int tube = 0; tube < all_tubes_size; tube++) {
            all_tubes[tube].set(0, color);
        }
    } else {
        tubes_off();
    }
} 

/**
 * @brief Creates a "flow" effect across tubes, starting from the first tube and moving sequentially.
 * 
 * This function simulates a flow effect by incrementally lighting up each tube in sequence, starting from the first tube
 * immediately after a beat is detected. The brightness of each tube is initially set to the maximum and then gradually
 * decreases until the next tube starts lighting up. The effect creates a visual representation of flow from one end to the other.
 */
void TubeController::flow() {
    static const uint8_t num_steps = 5;
    static unsigned int flow_delay = 50;
    static unsigned long local_beat_time = 0;
    static uint8_t tubes_step[MAX_TUBES] = {0};

    if (beat_indicator) {
        unsigned int time_between_beats = millis() - local_beat_time;
        flow_delay = time_between_beats / all_tubes_size;
        local_beat_time = millis();
    }
    tubes_off();

    unsigned int time_since_beat = millis() - local_beat_time;
    unsigned int tube_index = (time_since_beat / flow_delay);
    if(tube_index < all_tubes_size) { //only set tube brightness if the tube index is within bounds
        tubes_step[tube_index] = 0;
    }
        
    for (int tube = 0; tube < all_tubes_size; tube++) {
        if (tubes_step[tube] <= num_steps) {
            uint8_t tmp_red = interpolate(color.r, 0, tubes_step[tube], num_steps);
            uint8_t tmp_green = interpolate(color.g, 0, tubes_step[tube], num_steps);
            uint8_t tmp_blue = interpolate(color.b, 0, tubes_step[tube], num_steps);
            
            if(tubes_step[tube] < num_steps) {
                tubes_step[tube]++;  
            }    
            all_tubes[tube].set(0, CRGB(tmp_red, tmp_green, tmp_blue));
        }
    }
}

/**
 * @brief Creates a reverse flow effect across tubes, starting from the last tube and moving sequentially towards the first.
 * 
 * This function simulates a reverse flow effect by incrementally lighting up each tube in sequence, starting from the last tube
 * immediately after a beat is detected. The brightness of each tube is initially set to the maximum and then gradually
 * decreases until the next tube starts lighting up. The effect creates a visual representation of flow moving in the opposite direction.
 */
void TubeController::reverse_flow() {
    static const uint8_t num_steps = 5;
    static unsigned int flow_delay = 50;
    static unsigned long local_beat_time = 0;
    static uint8_t tubes_step[MAX_TUBES] = {0};

    if (beat_indicator) {
        unsigned int time_between_beats = millis() - local_beat_time;
        flow_delay = time_between_beats / all_tubes_size;
        local_beat_time = millis();
    }
    tubes_off();

    unsigned int time_since_beat = millis() - local_beat_time;
    unsigned int tube_index = all_tubes_size - 1 - (time_since_beat / flow_delay);
    if(tube_index < all_tubes_size) { //only set tube brightness if the tube index is within bounds
        tubes_step[tube_index] = 0;
    }
        
    for (int tube = 0; tube < all_tubes_size; tube++) {
        if (tubes_step[tube] <= num_steps) {
            uint8_t tmp_red = interpolate(color.r, 0, tubes_step[tube], num_steps);
            uint8_t tmp_green = interpolate(color.g, 0, tubes_step[tube], num_steps);
            uint8_t tmp_blue = interpolate(color.b, 0, tubes_step[tube], num_steps);
            
            if(tubes_step[tube] < num_steps) {
                tubes_step[tube]++;  
            }    
            all_tubes[tube].set(0, CRGB(tmp_red, tmp_green, tmp_blue));
        }
    }
}

/**
 * @brief Alternates between flow and reverse flow effects across tubes in sync with beats.
 * 
 * This function alternates the direction of the flow effect with each beat detected. On one beat, it will simulate a flow effect
 * starting from the first tube and moving sequentially. On the next beat, it will reverse the direction, starting from the last tube
 * and moving towards the first. This creates a dynamic visual effect that syncs with the music.
 */
void TubeController::flow_alternate() {
    static const uint8_t num_steps = 5;
    static unsigned int flow_delay = 50;
    static unsigned long local_beat_time = 0;
    static uint8_t tubes_step[MAX_TUBES] = {0};
    static bool direction_forward = true;

    if (beat_indicator) {
        direction_forward = !direction_forward;
        unsigned int time_between_beats = millis() - local_beat_time;
        flow_delay = time_between_beats / all_tubes_size;
        local_beat_time = millis();
    }
    tubes_off();

    unsigned int time_since_beat = millis() - local_beat_time;
    
    unsigned int tube_index;
    if (direction_forward) {
        tube_index = (time_since_beat / flow_delay);
    } else {
        tube_index = all_tubes_size - 1 - (time_since_beat / flow_delay);
    }
    
    if(tube_index < all_tubes_size) { //only set tube brightness if the tube index is within bounds
        tubes_step[tube_index] = 0;
    }
        
    for (int tube = 0; tube < all_tubes_size; tube++) {
        if (tubes_step[tube] <= num_steps) {
            uint8_t tmp_red = interpolate(color.r, 0, tubes_step[tube], num_steps);
            uint8_t tmp_green = interpolate(color.g, 0, tubes_step[tube], num_steps);
            uint8_t tmp_blue = interpolate(color.b, 0, tubes_step[tube], num_steps);
            
            if(tubes_step[tube] < num_steps) {
                tubes_step[tube]++;  
            }    
            all_tubes[tube].set(0, CRGB(tmp_red, tmp_green, tmp_blue));
        }
    }
}

/**
 * @brief Generates a wave effect on the tubes with a specified delay.
 *
 * This function creates a wave effect on the tubes by incrementally activating the tubes
 * based on the elapsed time since the last reset. If the function is called after a longer
 * pause, all tubes are reset. Otherwise, a certain number of tubes are activated based on
 * the elapsed time since the last reset and the wave delay.
 */
void TubeController::wave(){
    static unsigned long last_func_call = millis();
    static unsigned long time_wave_start = 0;
    static const unsigned int wave_delay = 150;

    /* check if function was recently called, if not, reset all tubes */
    if(millis() - last_func_call > RESET_DELAY) {
        time_wave_start = millis();
        fill_solid(0, CRGB::Black);
    }else{ /* set the needed tubes to the waves effect with a required delay */
        unsigned int number_waving_tubes = (millis() - time_wave_start) / wave_delay;
        number_waving_tubes = number_waving_tubes > all_tubes_size ? all_tubes_size : number_waving_tubes;

        for(int i = 0; i < number_waving_tubes; i++){
            all_tubes[i].set(6, color);
        } 
    }
    last_func_call = millis();
}

//unfinished, pulse is bright for too long
void TubeController::pulse_wave(){
    static unsigned long last_func_call = millis();
    static unsigned long time_wave_start = 0;
    static const unsigned int wave_delay = 150;

    /* check if function was recently called, if not, reset all tubes */
    if(millis() - last_func_call > RESET_DELAY) {
        time_wave_start = millis();
        fill_solid(0, CRGB(0, 0, 0));
    }else{ /* set the needed tubes to the waves effect with a required delay */
        unsigned int number_waving_tubes = (millis() - time_wave_start) / wave_delay;
        number_waving_tubes = number_waving_tubes > all_tubes_size ? all_tubes_size : number_waving_tubes;

        for(int i = 0; i < number_waving_tubes; i++){
            all_tubes[i].set(1, color);
        } 
    }
    last_func_call = millis();
}

/**
 * @brief Creates a random flash effect on the tubes.
 * 
 * This function randomly selects one or two tubes and sets their brightness to a predefined level, creating a flash effect.
 * The brightness of all tubes is then gradually decreased over time, simulating a fading effect. The function ensures that
 * the flash effect occurs no more frequently than every 10 milliseconds to prevent overly rapid flashing.
 */
void TubeController::random_flash(){
    static const uint8_t num_steps = 8;
    static unsigned long last_flash = 0;
    static uint8_t tubes_step[MAX_TUBES];
    
    unsigned int flash_delay = 60000 / sound.get_bpm();
    if(millis() - last_flash > flash_delay) {
        uint8_t flashing_tube = random(all_tubes_size);
        uint8_t flashing_tube2 = random(all_tubes_size);
        tubes_step[flashing_tube] = 0;
        tubes_step[flashing_tube2] = 0;

        last_flash = millis();
    }
    /* decrease the brightness of every tube and set it up*/
    for( int tube = 0; tube < all_tubes_size; tube++) {
        if(tubes_step[tube] <= num_steps) {
            uint8_t tmp_red = interpolate(color.r, 0, tubes_step[tube], num_steps);
            uint8_t tmp_green = interpolate(color.g, 0, tubes_step[tube], num_steps);
            uint8_t tmp_blue = interpolate(color.b, 0, tubes_step[tube], num_steps);
            
            if (tubes_step[tube] < num_steps) { tubes_step[tube]++; }
         
            all_tubes[tube].set(0, CRGB(tmp_red, tmp_green, tmp_blue));
        }
    }
}

/**
 * @brief Toggles between two colors on the LED strip with a specified delay.
 *
 * This function alternates the color of the LED strip between two specified colors
 * every time the specified delay has passed. The color change is based on the HSV
 * (Hue, Saturation, Value) color model. The function uses static variables to maintain
 * the toggle state and the time of the last color change across function calls.
 *
 * @param hue1 The hue component of the first color (0-255).
 * @param sat1 The saturation component of the first color (0-255).
 * @param val1 The value (brightness) component of the first color (0-255).
 * @param hue2 The hue component of the second color (0-255).
 * @param sat2 The saturation component of the second color (0-255).
 * @param val22 The value (brightness) component of the second color (0-255).
 */
void TubeController::toggle( CRGB color1, CRGB color2) {
    static bool toggle = false;
    const int delay = 70;
    static unsigned long time_of_color_change = 0;

    if(millis() - time_of_color_change > delay){
        time_of_color_change = millis();
        toggle = !toggle;
    }
    toggle ? fill_solid(0, color1) : fill_solid(0, color2);
}

/**
 * @brief Spreads one hue rotation across the row of tubes and lets it travel.
 *
 * The other choreographies move a single colour between tubes; this one gives
 * every tube its own colour, so the whole installation reads as one gradient.
 * The menu colour is ignored, as in the per-tube rainbow.
 */
void TubeController::rainbow_row() {
    static const uint16_t rainbow_period_ms = 6000; // one full rotation over the row

    uint8_t base = (millis() % rainbow_period_ms) * 256 / rainbow_period_ms;

    for (uint8_t tube = 0; tube < all_tubes_size; tube++) {
        CRGB rgb;
        hsv2rgb_rainbow(CHSV((uint8_t)(base + tube * 256 / all_tubes_size), 255, 255), rgb);
        all_tubes[tube].set(LED_SOLID, rgb);
    }
}

/**
 * @brief A light travelling across the row, trailing tubes that fade out behind it.
 *
 * Unlike flow(), which restarts on every beat and steps its fade in five coarse
 * jumps, this runs one smooth pass per bar. Both effects chase along the row, but
 * this one reads as a single moving object rather than as tubes switching over.
 */
void TubeController::row_comet() {
    static const uint8_t tail_tubes = 4;

    /* One pass per bar of four beats, so the comet stays locked to the music. */
    unsigned long bar_ms = 4UL * 60000UL / max((uint8_t)1, sound.get_bpm());
    unsigned long travel = all_tubes_size + tail_tubes;
    int16_t head = (millis() % bar_ms) * travel / bar_ms;

    tubes_off();

    for (uint8_t distance = 0; distance < tail_tubes; distance++) {
        int16_t index = head - distance;
        if (index < 0 || index >= all_tubes_size) continue;

        all_tubes[index].set(LED_SOLID, CRGB(interpolate(color.r, 0, distance, tail_tubes),
                                             interpolate(color.g, 0, distance, tail_tubes),
                                             interpolate(color.b, 0, distance, tail_tubes)));
    }
}

/**
 * @brief Lights tube pairs from the middle of the row outwards, one pair per beat.
 *
 * The pair fades over the beat that follows it, so the row opens up symmetrically
 * instead of the halves alternating the way beat_half_swap() does.
 */
void TubeController::center_out() {
    static const uint8_t num_steps = 5;
    static uint8_t ring = 0;

    if (beat_indicator) {
        ring++;
        if (ring > middle_index()) ring = 0;
    }
    tubes_off();

    unsigned long beat_duration = 60000UL / max((uint8_t)1, sound.get_bpm());
    unsigned long since_beat = millis() - beat_time;
    uint8_t step = (since_beat >= beat_duration) ? num_steps : (since_beat * num_steps / beat_duration);

    CRGB faded = CRGB(interpolate(color.r, 0, step, num_steps),
                      interpolate(color.g, 0, step, num_steps),
                      interpolate(color.b, 0, step, num_steps));

    int16_t upper = middle_index() + ring;
    int16_t lower = middle_index() - 1 - ring;

    if (upper < all_tubes_size) all_tubes[upper].set(LED_SOLID, faded);
    if (lower >= 0)             all_tubes[lower].set(LED_SOLID, faded);
}

/**
 * @brief Fills the row tube by tube over eight beats, then releases in a strobe.
 *
 * Gives the choreographies a shape that spans more than one beat, which is what
 * the beat-locked effects lack: tension over eight beats, then the release.
 */
void TubeController::build_up() {
    static const uint8_t beats_per_build = 8;
    static uint8_t beat_count = 0;

    if (beat_indicator) beat_count = (beat_count + 1) % (beats_per_build + 1);

    tubes_off();

    if (beat_count == beats_per_build) { // the release
        fill_solid(LED_STROBO, color);
        return;
    }

    uint8_t lit = (uint16_t)all_tubes_size * (beat_count + 1) / beats_per_build;
    for (uint8_t tube = 0; tube < lit; tube++) {
        all_tubes[tube].set(LED_SOLID, color);
    }
}

/**
 * @brief Cycles through a predefined set of colors at a fixed interval.
 * 
 * This function changes the `color` variable to a new value from a predefined array of color codes at regular intervals
 * specified by `color_periode`. The selection of the new color is random.
 */
void TubeController::cycle_color() {
    static unsigned long last_color_change = 0;
    static unsigned int color_periode = 5000;

    if(millis() - last_color_change > color_periode) {
        color.red = random(255);
        color.green = random(255);
        color.blue = random(255);

        last_color_change = millis();
    }
}

/**
 * @brief Cycles through a predefined set of effects at a fixed interval.
 * 
 * This function updates the `effect` variable to a new value from a predefined array of effect codes at regular intervals
 * specified by `effect_periode`. The selection of the new effect is random.
 */
void TubeController::cycle_effect() {
    static uint8_t effects[] = {1, 8, 9, 11, 12, 13, 15, 16, 17, 18, 19, 23, 30};
    static const uint8_t effects_size = sizeof(effects) / sizeof(effects[0]);
    static unsigned long last_effect_change = 0;
    static const unsigned int effect_periode = 5000;

    if(millis() - last_effect_change > effect_periode) {
        effect = effects[random(effects_size)];
        last_effect_change = millis();
    }
}

void TubeController::handle() {
    set_beat();

    switch(effect){
    case TUBE_FILL_FAST:
        fill_solid(LED_PULSE_FAST, color);
        break;
    case TUBE_BEAT_FLASH:
        beat_flash();
        break;
    case TUBE_FILL_FAST_2:
        fill_solid(LED_PULSE_FAST, color);
        break;
    case TUBE_WAVE:
        wave();
        break;
    case TUBE_PULSE_WAVE:
        pulse_wave();
        break;
    case TUBE_FLOW:
        flow();
        break;
    case TUBE_REVERSE_FLOW:
        reverse_flow();
        break;
    case TUBE_FLOW_ALTERNATE:
        flow_alternate();
        break;
    case TUBE_STROBO:
        strobo();
        break;
    case TUBE_BEAT_HALF_SWAP:
        beat_half_swap();
        break;
    case TUBE_DOUBLE_BEAT_HALF_SWAP:
        double_beat_half_swap();
        break;
    case TUBE_BEAT_FLASH_ALTERNATE:
        beat_flash_alternate();
        break;
    case TUBE_DOUBLE_BEAT_FLASH_ALTERNATE:
        double_beat_flash_alternate();
        break;
    case TUBE_TOGGLE_RED_BLUE:
        toggle( CRGB(255, 0 , 0), CRGB(0, 0 ,255));   // red - blue
        break;
    case TUBE_TOGGLE_BLACK_PURPLE:
        toggle( CRGB(0, 0, 0), CRGB(70, 0, 255));     // black - purple
        break;
    case TUBE_RANDOM_FLASH:
        random_flash();
        break;
    case TUBE_RAINBOW_ROW:
        rainbow_row();
        break;
    case TUBE_ROW_COMET:
        row_comet();
        break;
    case TUBE_CENTER_OUT:
        center_out();
        break;
    case TUBE_BUILD_UP:
        build_up();
        break;
    case TUBE_ALL_PULSE:
        fill_solid(LED_PULSE, color);
        break;
    case TUBE_ALL_PULSE_SLOW:
        fill_solid(LED_PULSE_SLOW, color);
        break;
    case TUBE_ALL_PULSE_FAST:
        fill_solid(LED_PULSE_FAST, color);
        break;
    case TUBE_ALL_BOTTOM_TO_TOP:
        fill_solid(LED_BOTTOM_TO_TOP, color);
        break;
    case TUBE_ALL_TOP_TO_BOTTOM:
        fill_solid(LED_TOP_TO_BOTTOM, color);
        break;
    case TUBE_ALL_UP_AND_DOWN:
        fill_solid(LED_UP_AND_DOWN, color);
        break;
    case TUBE_ALL_STROBO:
        fill_solid(LED_STROBO, color);
        break;
    case TUBE_ALL_RAINBOW:
        fill_solid(LED_RAINBOW, color);
        break;
    case TUBE_ALL_SELECTED:
        fill_solid(tube_effect, color);
        break;
    default:
        fill_solid(LED_PULSE_FAST, color);
        break;
    }
    generate_payload();
    beat_indicator = false;
}