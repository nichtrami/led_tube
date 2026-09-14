#include "LedController.hpp"

/* ================= TIMING ================= */
static constexpr uint8_t TARGET_FPS        = 60;
static constexpr uint8_t FRAME_INTERVAL_MS = 1000 / TARGET_FPS; // 16 ms

static constexpr uint16_t SWEEP_PERIOD_MS = 2000; // block travels the strip once
static constexpr uint8_t  SWEEP_LENGTH    = 20;   // LEDs lit by the sweeping block

/** @brief Registers the strip with FastLED. Call from setup(). */
void LedController::begin() {
    FastLED.addLeds<WS2812B, LED_DATA_PIN, RGB>(leds, NUM_LEDS); // WS2412
    //FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_LEDS); // WS2814

    Serial.println("LED Controller initialized");
}

/**
 * @brief Switches to a new effect and restarts its animation phase.
 *
 * Resetting the time base is what makes an effect start at its beginning. Strip
 * and heat buffer are cleared too, so fire() and sparkle() -- the only effects
 * building on the previous frame -- do not inherit the picture before them.
 *
 * @param new_effect The effect ID to switch to.
 */
void LedController::set_effect(uint8_t new_effect) {
    if (new_effect == effect) return;

    effect = new_effect;
    effect_start_ms = millis();

    memset(heat, 0, sizeof(heat));
    fill_solid(leds, NUM_LEDS, CRGB::Black);
}

void LedController::set_input(CRGB input_color) {
    set_effect(LED_SOLID);
    color.r = input_color.r;
    color.g = input_color.g;
    color.b = input_color.b;
}

/**
 * @brief Sets the controller's state from a received tube dataset.
 *
 * @param input A TubeInput struct. Including effect, red, green, and blue values.
 */
void LedController::set_input(TubeInput input) {
    set_effect(input.effect);
    color.r = input.red;
    color.g = input.green;
    color.b = input.blue;
}

/**
 * @brief Sets LED pixel data directly from external input.
 *
 * This method copies pixel data from the input buffer to the internal LED array.
 * It is currently only used by the bridge device in artnet mode.
 *
 * @param input PixelControlInput struct containing pixel data and size
 * @note Currently only used by the bridge device
 */
void LedController::set_input(PixelControlInput input) {
    uint16_t bytes_to_copy = sizeof(CRGB) * min(input.size, (uint16_t)NUM_LEDS);
    memcpy(leds, input.pixels, bytes_to_copy);
}


uint8_t LedController::exponential_increase(uint8_t x){
    float normalized_x = x / 255.0f;
    float squared = normalized_x * normalized_x;
    float y = squared * squared * 255.0f;
    return static_cast<uint8_t>(y);
}

void LedController::fill_solid(struct CRGB * target_array, int num_to_fill, const struct CRGB& color) {
     for( int i = 0; i < num_to_fill; ++i) {
        target_array[i] = color;
    }
}

/* ================= DRAWING PRIMITIVES ================= */

void LedController::clear() {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
}

/** @brief The selected color, scaled to @p brightness. */
CRGB LedController::scaled_color(uint8_t brightness) {
    return CRGB(color.r * brightness / 255, color.g * brightness / 255, color.b * brightness / 255);
}

/**
 * @brief Position of the sweeping block for a given phase.
 *
 * @param phase     Elapsed time within the current sweep, in [0, period_ms).
 * @param period_ms Duration of one full travel across the strip.
 * @return Index of the block's lower end, running from -SWEEP_LENGTH to NUM_LEDS.
 */
int16_t LedController::sweep_position(uint32_t phase, uint16_t period_ms) {
    const uint16_t travel = NUM_LEDS + SWEEP_LENGTH;
    return static_cast<int16_t>((phase * travel) / period_ms) - SWEEP_LENGTH;
}

/**
 * @brief Fills a block of SWEEP_LENGTH LEDs starting at @p position.
 *
 * The block is clipped to the strip, so positions that reach partly or fully
 * outside it are safe.
 *
 * @param position Index of the block's lower end. May be negative.
 */
void LedController::draw_block(int16_t position) {
    int16_t start = position < 0 ? 0 : position;
    int16_t end   = position + SWEEP_LENGTH;
    if (end > NUM_LEDS) end = NUM_LEDS;

    if (end > start) fill_solid(&leds[start], end - start, color);
}

/**
 * @brief Draws a moving head with a tail fading out behind it.
 *
 * The tail is computed from the head position alone rather than by dimming the
 * previous frame, which keeps the effects that use it pure functions of the
 * elapsed time. Pixels outside the strip are skipped, so a head that has already
 * left the tube still drags its tail out correctly.
 *
 * @param head      Index of the brightest pixel. May lie outside the strip.
 * @param length    Distance from the head to the dark end of the tail, in LEDs.
 * @param direction Direction of travel: +1 towards the top, -1 towards the bottom.
 *                  The tail trails against it.
 */
void LedController::draw_tail(int16_t head, uint8_t length, int8_t direction) {
    for (uint8_t distance = 0; distance < length; ++distance) {
        int16_t index = head - direction * static_cast<int16_t>(distance);
        if (index < 0 || index >= NUM_LEDS) continue;

        uint8_t ramp = 255 - static_cast<uint8_t>(distance * 255 / length);
        leds[index] = scaled_color(exponential_increase(ramp));
    }
}

/**
 * @brief Builds the ramp fire() maps its heat through.
 *
 * Runs black -> dimmed color -> color -> washed out towards white, so the RGB
 * channels still choose the character of the flame instead of being ignored the
 * way a fixed heat palette would.
 */
CRGBPalette16 LedController::fire_palette() {
    CRGB dim  = scaled_color(70);
    CRGB hot  = color;
    CRGB peak = CRGB(qadd8(color.r, 160), qadd8(color.g, 160), qadd8(color.b, 160));

    return CRGBPalette16(CRGB::Black, dim, hot, peak);
}

/* ================= EFFECTS ================= */

void LedController::solid_color_mode() {
    fill_solid(leds, NUM_LEDS, color);
}

/**
 * @brief Smoothly pulses all LEDs up and down in brightness.
 *
 * The brightness follows a triangle across @p period_ms, shaped by
 * exponential_increase() so the ramp is perceived as even.
 *
 * @param elapsed   Time since the effect was selected, in ms.
 * @param period_ms Duration of one full up+down cycle.
 */
void LedController::pulse(uint32_t elapsed, uint16_t period_ms) {
    uint32_t phase = elapsed % period_ms;
    uint32_t half  = period_ms / 2;

    uint8_t ramp = (phase < half)
        ? static_cast<uint8_t>(phase * 255 / half)
        : static_cast<uint8_t>((period_ms - phase) * 255 / (period_ms - half));

    fill_solid(leds, NUM_LEDS, scaled_color(exponential_increase(ramp)));
}

void LedController::bottom_to_top(uint32_t elapsed) {
    clear();
    draw_block(sweep_position(elapsed % SWEEP_PERIOD_MS, SWEEP_PERIOD_MS));
}

void LedController::top_to_bottom(uint32_t elapsed) {
    clear();

    int16_t position = sweep_position(elapsed % SWEEP_PERIOD_MS, SWEEP_PERIOD_MS);
    draw_block(NUM_LEDS - SWEEP_LENGTH - position); // mirrored along the strip
}

void LedController::up_and_down(uint32_t elapsed) {
    clear();

    const uint32_t cycle = 2UL * SWEEP_PERIOD_MS;
    uint32_t phase = elapsed % cycle;
    if (phase >= SWEEP_PERIOD_MS) phase = cycle - phase; // mirror the second half

    draw_block(sweep_position(phase, SWEEP_PERIOD_MS));
}

void LedController::strobo(uint32_t elapsed) {
    static constexpr uint16_t STROBE_PERIOD_MS = 100; // 10 Hz
    static constexpr uint16_t STROBE_ON_MS     = 20;  // 20 % duty cycle

    bool on = (elapsed % STROBE_PERIOD_MS) < STROBE_ON_MS;
    fill_solid(leds, NUM_LEDS, on ? color : CRGB(0, 0, 0));
}

/**
 * @brief Cycles the whole strip through the hue wheel.
 *
 * @param elapsed Time since the effect was selected, in ms.
 */
void LedController::rainbow_solid(uint32_t elapsed) {
    static constexpr uint16_t RAINBOW_PERIOD_MS = 10000; // one full hue rotation

    uint8_t hue = static_cast<uint8_t>((elapsed % RAINBOW_PERIOD_MS) * 256 / RAINBOW_PERIOD_MS);

    uint8_t red, green, blue;
    hsv2rgb_rainbow(hue, 255, 255, red, green, blue);
    fill_solid(leds, NUM_LEDS, CRGB(red, green, blue));
}

/**
 * @brief Lays one full hue rotation along the tube and scrolls it upwards.
 *
 * Unlike rainbow_solid(), which keeps the whole strip on one hue, this shows the
 * tube's length: the colour differs from LED to LED and the whole gradient
 * travels.
 *
 * @param period_ms Time for the gradient to travel its own length once.
 */
void LedController::rainbow_flow(uint32_t elapsed, uint16_t period_ms) {
    uint8_t offset = static_cast<uint8_t>((elapsed % period_ms) * 256 / period_ms);

    for (uint8_t i = 0; i < NUM_LEDS; ++i) {
        uint8_t hue = offset + static_cast<uint8_t>(i * 256 / NUM_LEDS);

        uint8_t red, green, blue;
        hsv2rgb_rainbow(hue, 255, 255, red, green, blue);
        leds[i] = CRGB(red, green, blue);
    }
}

/**
 * @brief A head travelling from the bottom to the top, dragging a fading tail.
 *
 * The head runs past the end of the strip so the tail leaves the tube completely
 * before the next one enters at the bottom.
 */
void LedController::comet(uint32_t elapsed, uint16_t period_ms) {
    static constexpr uint8_t COMET_LENGTH = 25; // LEDs from the head to the end of the tail

    clear();

    const uint16_t travel = NUM_LEDS + COMET_LENGTH;
    int16_t head = static_cast<int16_t>(((elapsed % period_ms) * travel) / period_ms);

    draw_tail(head, COMET_LENGTH, +1);
}

/**
 * @brief A comet head that ping-pongs between the ends of the tube.
 *
 * The tail always trails behind the current direction of travel, so it flips at
 * both turning points.
 *
 * @param period_ms Duration of one single pass; a full cycle takes twice as long.
 */
void LedController::scanner(uint32_t elapsed, uint16_t period_ms) {
    static constexpr uint8_t SCANNER_LENGTH = 18; // LEDs from the head to the end of the tail

    clear();

    const uint32_t cycle = 2UL * period_ms;
    uint32_t phase = elapsed % cycle;

    bool upward = phase < period_ms;
    if (!upward) phase = cycle - phase; // mirror the second half

    int16_t head = static_cast<int16_t>((phase * (NUM_LEDS - 1)) / period_ms);
    draw_tail(head, SCANNER_LENGTH, upward ? +1 : -1);
}

/**
 * @brief Two heads leaving the middle of the tube towards both ends.
 *
 * @param period_ms Time for a pair to travel from the middle out of the tube.
 */
void LedController::center_out(uint32_t elapsed, uint16_t period_ms) {
    static constexpr uint8_t CENTER_LENGTH = 15; // LEDs from a head to the end of its tail

    clear();

    const uint8_t half = NUM_LEDS / 2;
    const uint16_t travel = half + CENTER_LENGTH;
    int16_t offset = static_cast<int16_t>(((elapsed % period_ms) * travel) / period_ms);

    draw_tail(half + offset, CENTER_LENGTH, +1);
    draw_tail(half - 1 - offset, CENTER_LENGTH, -1);
}

/**
 * @brief Heat rising from the bottom of the tube.
 *
 * Every frame each cell loses a little heat, the remaining heat drifts upwards
 * and blurs, and new sparks are lit near the bottom. Unlike the other effects
 * this builds on the previous frame; set_effect() clears heat[] so a fresh
 * selection always starts from a cold tube.
 *
 * @param cooling  How fast a cell loses heat. Higher means shorter flames.
 * @param sparking Chance out of 255 that a new spark is lit per frame.
 */
void LedController::fire(uint8_t cooling, uint8_t sparking) {
    for (uint8_t i = 0; i < NUM_LEDS; ++i) {
        heat[i] = qsub8(heat[i], random8(0, ((cooling * 10) / NUM_LEDS) + 2));
    }

    for (uint8_t i = NUM_LEDS - 1; i >= 2; --i) {
        heat[i] = (heat[i - 1] + heat[i - 2] + heat[i - 2]) / 3;
    }

    if (random8() < sparking) {
        uint8_t i = random8(7);
        heat[i] = qadd8(heat[i], random8(160, 255));
    }

    CRGBPalette16 palette = fire_palette();
    for (uint8_t i = 0; i < NUM_LEDS; ++i) {
        leds[i] = ColorFromPalette(palette, scale8(heat[i], 240));
    }
}

/**
 * @brief Perlin noise drifting along the tube, rendered in the selected colour.
 *
 * Reads as a slow organic glow rather than as a repeating pattern, because the
 * noise field never returns to the same state.
 *
 * @param speed How fast the field travels through the tube.
 */
void LedController::plasma(uint32_t elapsed, uint8_t speed) {
    static constexpr uint16_t PLASMA_SCALE = 4000; // noise units per LED, in 16.16 fixed point

    uint32_t time = elapsed * speed;

    for (uint8_t i = 0; i < NUM_LEDS; ++i) {
        uint8_t raw = inoise16(static_cast<uint32_t>(i) * PLASMA_SCALE, time) >> 8;

        /* Raw noise clusters around the middle of the range; stretching it gives
           the effect real darks and real highlights. */
        raw = qsub8(raw, 16);
        leds[i] = scaled_color(qadd8(raw, scale8(raw, 39)));
    }
}

/**
 * @brief A ball dropped from the top, bouncing off the bottom and losing height.
 *
 * Each bounce is a parabola whose peak and duration come from the BOUNCE_* tables;
 * the shorter the bounce, the faster it is, which is what sells the gravity.
 * After the last bounce the ball is dropped again.
 *
 * @param period_ms Duration of one full drop-and-settle cycle.
 */
void LedController::bounce(uint32_t elapsed, uint16_t period_ms) {
    static constexpr uint8_t BOUNCE_LENGTH = 10; // LEDs from the ball to the end of its tail

    /* Peak height of each bounce in percent of the tube, and the share of the
       cycle that bounce takes. The times follow the square root of the heights,
       which is what makes the ball read as falling rather than as sliding. */
    static constexpr uint8_t BOUNCE_HEIGHT_PCT[] = {100, 60, 36, 21, 12};
    static constexpr uint8_t BOUNCE_TIME_PCT[]   = { 32, 24, 19, 14, 11};
    static constexpr uint8_t BOUNCE_COUNT        = sizeof(BOUNCE_HEIGHT_PCT);

    clear();

    uint32_t phase = elapsed % period_ms;

    /* Locate the bounce the ball is currently in. The last one absorbs whatever
       the percentages leave over, so `phase` can never fall past the table. */
    uint8_t  index = 0;
    uint32_t start = 0;
    uint32_t span  = static_cast<uint32_t>(period_ms) * BOUNCE_TIME_PCT[0] / 100;

    while (index + 1 < BOUNCE_COUNT && phase >= start + span) {
        start += span;
        ++index;
        span = static_cast<uint32_t>(period_ms) * BOUNCE_TIME_PCT[index] / 100;
    }
    if (span == 0) span = 1;

    /* Parabola: on the ground at both ends of the bounce, at its peak in between.
       The spans round down, so the last bounce keeps a few ms of remainder; the
       ball simply rests on the ground for them instead of overshooting the curve. */
    uint32_t position_in_bounce = phase - start;
    if (position_in_bounce > span) position_in_bounce = span;

    int32_t  offset = static_cast<int32_t>(2 * position_in_bounce * 255 / span) - 255;
    uint8_t  height = 255 - static_cast<uint8_t>((offset * offset) / 255);

    uint8_t peak = (NUM_LEDS - 1) * BOUNCE_HEIGHT_PCT[index] / 100;
    int16_t head = static_cast<int16_t>(static_cast<uint32_t>(peak) * height / 255);

    draw_tail(head, BOUNCE_LENGTH, offset < 0 ? +1 : -1);
}

/**
 * @brief Fills the tube from the bottom, then clears it from the bottom again.
 *
 * @param period_ms Duration of the fill; the clear takes just as long.
 */
void LedController::wipe(uint32_t elapsed, uint16_t period_ms) {
    const uint32_t cycle = 2UL * period_ms;
    uint32_t phase = elapsed % cycle;

    if (phase < period_ms) {
        uint8_t filled = (phase * NUM_LEDS) / period_ms;
        clear();
        fill_solid(leds, filled, color);
    } else {
        uint8_t cleared = ((phase - period_ms) * NUM_LEDS) / period_ms;
        fill_solid(leds, NUM_LEDS, color);
        fill_solid(leds, cleared, CRGB::Black);
    }
}

/**
 * @brief A static brightness gradient. The only effect that does not animate.
 *
 * @param from_center true puts the bright end in the middle of the tube and
 *                    fades towards both ends, false fades from bottom to top.
 */
void LedController::gradient(bool from_center) {
    const uint8_t half = NUM_LEDS / 2;
    const uint8_t span = from_center ? half : NUM_LEDS;

    for (uint8_t i = 0; i < NUM_LEDS; ++i) {
        uint8_t distance = from_center ? (i < half ? half - 1 - i : i - half) : i;
        uint8_t ramp = 255 - static_cast<uint8_t>(static_cast<uint16_t>(distance) * 255 / span);

        leds[i] = scaled_color(exponential_increase(ramp));
    }
}

/**
 * @brief Random pixels flash up in the selected colour and fade out again.
 *
 * Builds on the previous frame for the fade; set_effect() clears the strip so
 * a fresh selection does not inherit the picture of the effect before it.
 *
 * @param chance Chance out of 255 per draw that a new sparkle is lit.
 */
void LedController::sparkle(uint8_t chance) {
    static constexpr uint8_t SPARKLE_FADE  = 40; // brightness a sparkle loses per frame
    static constexpr uint8_t SPARKLE_TRIES = 3;  // draws per frame, each with @p chance

    fadeToBlackBy(leds, NUM_LEDS, SPARKLE_FADE);

    for (uint8_t i = 0; i < SPARKLE_TRIES; ++i) {
        if (random8() < chance) leds[random8(NUM_LEDS)] = color;
    }
}

/**
 * @brief Irregular bursts of one to three hard flashes.
 *
 * Time is cut into slots of @p gap_ms and the random generator is reseeded from
 * the slot number, so every frame of a slot reconstructs the same burst. That
 * keeps the effect a pure function of `elapsed` despite looking random.
 *
 * @param gap_ms Distance between two bursts.
 */
void LedController::lightning(uint32_t elapsed, uint16_t gap_ms) {
    clear();

    uint32_t slot  = elapsed / gap_ms;
    uint32_t phase = elapsed % gap_ms;

    random16_set_seed(static_cast<uint16_t>(slot * 2053 + 1));
    uint16_t start    = random16(gap_ms / 2); // when inside the slot the burst hits
    uint8_t  flashes  = random8(1, 4);
    uint8_t  duration = random8(20, 60);      // ms per flash, and per gap between them

    if (phase < start) return;

    uint32_t since = phase - start;
    if (since / (duration * 2u) >= flashes) return;

    if ((since % (duration * 2u)) < duration) fill_solid(leds, NUM_LEDS, color);
}

/**
 * @brief Blocks of light marching along the tube.
 *
 * @param period_ms Time for a block to move on by one full spacing.
 */
void LedController::chase(uint32_t elapsed, uint16_t period_ms) {
    static constexpr uint8_t CHASE_ON      = 6;  // lit LEDs per marching block
    static constexpr uint8_t CHASE_SPACING = 18; // distance between two blocks

    clear();

    int16_t offset = static_cast<int16_t>((elapsed % period_ms) * CHASE_SPACING / period_ms);

    /* Start one spacing below the strip so the block entering at the bottom is
       drawn partially instead of popping into view. */
    for (int16_t start = offset - CHASE_SPACING; start < NUM_LEDS; start += CHASE_SPACING) {
        int16_t begin = start < 0 ? 0 : start;
        int16_t end   = start + CHASE_ON;
        if (end > NUM_LEDS) end = NUM_LEDS;

        if (end > begin) fill_solid(&leds[begin], end - begin, color);
    }
}

/** @brief Renders one frame of the selected effect, if the frame interval has elapsed. */
void LedController::handle() {
    unsigned long now = millis();
    if (now - last_frame_ms < FRAME_INTERVAL_MS) return;
    last_frame_ms = now;

#if DEVICE_MODE == DEVICE_MODE_TUBE
    const uint32_t elapsed = now - effect_start_ms;

    switch (effect) {
        case LED_SOLID:             solid_color_mode();           break;
        case LED_PULSE:             pulse(elapsed, 2000);         break;
        case LED_PULSE_SLOW:        pulse(elapsed, 4000);         break;
        case LED_PULSE_FAST:        pulse(elapsed,  800);         break;
        case LED_BOTTOM_TO_TOP:     bottom_to_top(elapsed);       break;
        case LED_TOP_TO_BOTTOM:     top_to_bottom(elapsed);       break;
        case LED_UP_AND_DOWN:       up_and_down(elapsed);         break;
        case LED_STROBO:            strobo(elapsed);              break;

        case LED_RAINBOW:           rainbow_solid(elapsed);       break;
        case LED_RAINBOW_FLOW_SLOW: rainbow_flow(elapsed, 12000); break;
        case LED_RAINBOW_FLOW:      rainbow_flow(elapsed,  6000); break;
        case LED_RAINBOW_FLOW_FAST: rainbow_flow(elapsed,  2500); break;

        case LED_COMET_SLOW:        comet(elapsed, 4000);         break;
        case LED_COMET:             comet(elapsed, 2000);         break;
        case LED_COMET_FAST:        comet(elapsed,  900);         break;

        case LED_SCANNER_SLOW:      scanner(elapsed, 3000);       break;
        case LED_SCANNER:           scanner(elapsed, 1500);       break;
        case LED_SCANNER_FAST:      scanner(elapsed,  700);       break;

        case LED_CENTER_OUT_SLOW:   center_out(elapsed, 3000);    break;
        case LED_CENTER_OUT:        center_out(elapsed, 1500);    break;
        case LED_CENTER_OUT_FAST:   center_out(elapsed,  700);    break;

        case LED_FIRE_CALM:         fire(85,  60);                break; // cooling, sparking
        case LED_FIRE:              fire(60, 110);                break;
        case LED_FIRE_WILD:         fire(40, 180);                break;

        case LED_PLASMA_SLOW:       plasma(elapsed, 12);          break;
        case LED_PLASMA:            plasma(elapsed, 32);          break;
        case LED_PLASMA_FAST:       plasma(elapsed, 80);          break;

        case LED_BOUNCE_SLOW:       bounce(elapsed, 5000);        break;
        case LED_BOUNCE:            bounce(elapsed, 3200);        break;
        case LED_BOUNCE_FAST:       bounce(elapsed, 2000);        break;

        case LED_WIPE_SLOW:         wipe(elapsed, 3000);          break;
        case LED_WIPE:              wipe(elapsed, 1500);          break;
        case LED_WIPE_FAST:         wipe(elapsed,  700);          break;

        case LED_GRADIENT:          gradient(false);              break;
        case LED_GRADIENT_CENTER:   gradient(true);               break;

        case LED_SPARKLE_THIN:      sparkle( 8);                  break;
        case LED_SPARKLE:           sparkle(30);                  break;
        case LED_SPARKLE_DENSE:     sparkle(90);                  break;

        case LED_LIGHTNING_RARE:    lightning(elapsed, 1600);     break;
        case LED_LIGHTNING:         lightning(elapsed,  800);     break;
        case LED_LIGHTNING_WILD:    lightning(elapsed,  350);     break;

        case LED_CHASE_SLOW:        chase(elapsed, 1500);         break;
        case LED_CHASE:             chase(elapsed,  800);         break;
        case LED_CHASE_FAST:        chase(elapsed,  350);         break;

        default:                    solid_color_mode();           break;
    }
#endif

    FastLED.show();
}
