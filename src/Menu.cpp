#include "Menu.hpp"
#include "Display.hpp"
#include "LedController.hpp" // LedEffect IDs behind the per-tube effect options

Menu* Menu::instance = nullptr;

/* ================= MODE OPTIONS ================= */

#if DEVICE_MODE == DEVICE_MODE_BRIDGE
static const MenuOption mode_options[] = {
    {"Master", 0}, {"DMX", 1}, {"ArtNet", 2}
};
#elif DEVICE_MODE == DEVICE_MODE_TUBE
static const MenuOption mode_options[] = {
    {"Standalone", 0}, {"DMX", 1}
};
#endif

/* Sentinel value of a category option that selects automatic cycling. */
static const uint8_t AUTO_VALUE = 255;

static const MenuOption led_effect_options[] = {
    {"Solid", LED_SOLID}, {"Pulse", LED_PULSE}, {"Pulse Slow", LED_PULSE_SLOW}, {"Pulse Fast", LED_PULSE_FAST},
    {"Bottom Up", LED_BOTTOM_TO_TOP}, {"Top Down", LED_TOP_TO_BOTTOM}, {"Up & Down", LED_UP_AND_DOWN}, {"Strobo", LED_STROBO},
    {"Rainbow", LED_RAINBOW}, {"RB Flow Slow", LED_RAINBOW_FLOW_SLOW}, {"RB Flow", LED_RAINBOW_FLOW}, {"RB Flow Fast", LED_RAINBOW_FLOW_FAST},
    {"Comet Slow", LED_COMET_SLOW}, {"Comet", LED_COMET}, {"Comet Fast", LED_COMET_FAST},
    {"Scanner Slow", LED_SCANNER_SLOW}, {"Scanner", LED_SCANNER}, {"Scanner Fast", LED_SCANNER_FAST},
    {"Center Slow", LED_CENTER_OUT_SLOW}, {"Center", LED_CENTER_OUT}, {"Center Fast", LED_CENTER_OUT_FAST},
    {"Fire Calm", LED_FIRE_CALM}, {"Fire", LED_FIRE}, {"Fire Wild", LED_FIRE_WILD},
    {"Plasma Slow", LED_PLASMA_SLOW}, {"Plasma", LED_PLASMA}, {"Plasma Fast", LED_PLASMA_FAST},
    {"Bounce Slow", LED_BOUNCE_SLOW}, {"Bounce", LED_BOUNCE}, {"Bounce Fast", LED_BOUNCE_FAST},
    {"Wipe Slow", LED_WIPE_SLOW}, {"Wipe", LED_WIPE}, {"Wipe Fast", LED_WIPE_FAST},
    {"Gradient", LED_GRADIENT}, {"Gradient Mid", LED_GRADIENT_CENTER},
    {"Sparkle Thin", LED_SPARKLE_THIN}, {"Sparkle", LED_SPARKLE}, {"Sparkle Dense", LED_SPARKLE_DENSE},
    {"Lightning Rare", LED_LIGHTNING_RARE}, {"Lightning", LED_LIGHTNING}, {"Lightning Wild", LED_LIGHTNING_WILD},
    {"Chase Slow", LED_CHASE_SLOW}, {"Chase", LED_CHASE}, {"Chase Fast", LED_CHASE_FAST}
};

#if DEVICE_MODE == DEVICE_MODE_BRIDGE

static const MenuOption effect_options[] = {
    {"Auto", AUTO_VALUE}, {"Beat Flash", 1}, {"Wave", 3}, {"Flow", 5},
    {"Rev Flow", 6}, {"Strobo", 8}, {"Half Swap", 9}, {"Random", 15},
    {"RB Row", 16}, {"Row Comet", 17}, {"Center Out", 18}, {"Build Up", 19},
    {"All Tubes", 32}
};

static const MenuOption color_options[] = {
    {"Auto", AUTO_VALUE}, {"Red", 0}, {"Yellow", 1}, {"Orange", 2}, {"Green", 3},
    {"Cyan", 4}, {"Blue", 5}, {"Magenta", 6}, {"Purple", 7}, {"White", 8}
};

#else
static const MenuOption color_options[] = {
    {"Red", 0}, {"Yellow", 1}, {"Orange", 2}, {"Green", 3}, {"Cyan", 4},
    {"Blue", 5}, {"Magenta", 6}, {"Purple", 7}, {"White", 8}
};
#endif

static const CRGB color_values[] = {
    CRGB::Red, CRGB::Yellow, CRGB::Orange, CRGB::Green, CRGB::Cyan,
    CRGB::Blue, CRGB::Magenta, CRGB::Purple, CRGB::White
};

static const MenuOption dhcp_options[] = {
    {"Off", 0}, {"On", 1}
};

/* ================= CONSTRUCTOR ================= */

Menu::Menu()
:   item_mode(ITEM_MODE, "Mode", 0, mode_options, sizeof(mode_options) / sizeof(MenuOption)),
    item_addr(ITEM_ADDR, "Addr", 1, 1, 255),
#if DEVICE_MODE == DEVICE_MODE_BRIDGE
    item_color(ITEM_COLOR, "Color", AUTO_VALUE, color_options, sizeof(color_options) / sizeof(MenuOption)),
#else
    item_color(ITEM_COLOR, "Color", 0, color_options, sizeof(color_options) / sizeof(MenuOption)),
#endif
#if DEVICE_MODE == DEVICE_MODE_BRIDGE
    item_effect(ITEM_EFFECT, "FX", AUTO_VALUE, effect_options, sizeof(effect_options) / sizeof(MenuOption)),
#else
    item_effect(ITEM_EFFECT, "FX", 0, led_effect_options, sizeof(led_effect_options) / sizeof(MenuOption)),
#endif
    item_tube_fx(ITEM_TUBE_FX, "Tube FX", LED_PULSE_FAST, led_effect_options, sizeof(led_effect_options) / sizeof(MenuOption)),
    item_channel(ITEM_CHANNEL, "Channel", 1, 1, 3),
    item_tubes(ITEM_TUBES, "Tubes", 12, 1, 20),
    item_bpm(ITEM_BPM, "BPM", 125, 60, 200),
    item_pixels(ITEM_PIXELS, "Pixels", 60, 1, 2000),
    item_dhcp(ITEM_DHCP, "DHCP", 0, dhcp_options, sizeof(dhcp_options) / sizeof(MenuOption)),
    item_ip_o1(ITEM_IP_O1, "Octet 1", 192, 0, 255),
    item_ip_o2(ITEM_IP_O2, "Octet 2", 168, 0, 255),
    item_ip_o3(ITEM_IP_O3, "Octet 3", 1, 0, 255),
    item_ip_o4(ITEM_IP_O4, "Octet 4", 100, 0, 255),
    item_net(ITEM_NET, "Net", 0, 0, 127),
    item_subnet(ITEM_SUBNET, "Sub-Net", 0, 0, 15),
    item_universe(ITEM_UNIVERSE, "Universe", 0, 0, 15),
    menu_node_ip(ITEM_IP_ADDR, "IP", nullptr, 0)
{
    instance = this;
    current_mode = static_cast<OperatingMode>(item_mode.get_value());
    menu_depth = 0;
}

/* ================= BEGIN ================= */

/**
 * @brief Initializes display, buttons, and loads saved configuration
 * 
 * Sets up I2C communication, initializes OLED display, configures button pins
 * with pull-up resistors and interrupts, loads values from flash, and builds menu tree.
 */
void Menu::begin() {
    // Initialize I2C for display
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000); // 400kHz I2C clock. 
    Serial.println("I2C initialized");

    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 Display allocation failed"));
    }
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    Serial.println("Display initialized");

    // Initialize buttons with interrupts
    pinMode(PIN_UP, INPUT_PULLUP);
    pinMode(PIN_DOWN, INPUT_PULLUP);
    pinMode(PIN_ENTER, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(PIN_UP), handle_up_isr, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_DOWN), handle_down_isr, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_ENTER), handle_enter_isr, FALLING);
    Serial.println("Buttons initialized");

    load_from_flash();
    build_menu_tree();
    Serial.println("Menu initialized");
}

/* ================= MENU BUILDING ================= */

/**
 * @brief Builds menu structure based on current operating mode
 * 
 * Creates IP submenu node with dynamic display callback.
 * Populates root menu items array based on device mode and operating mode.
 * Resets navigation state and marks display for update.
 */
void Menu::build_menu_tree() {
    static MenuItem* ip_children[] = {&item_ip_o1, &back_item, &item_ip_o4, &item_ip_o3, &item_ip_o2};
    menu_node_ip = MenuNode(ITEM_IP_ADDR, "IP", ip_children, 5,
                          [this]() { 
                              return String("IP") + ": " +
                                     String(item_ip_o1.get_value()) + "." +
                                     String(item_ip_o2.get_value()) + "." +
                                     String(item_ip_o3.get_value()) + "." +
                                     String(item_ip_o4.get_value());
                          });

    // Build root menu based on current mode
    root_item_count = 0;
    root_items[root_item_count++] = &item_mode;
    root_items[root_item_count++] = &item_channel;

    switch (current_mode) {
#if DEVICE_MODE == DEVICE_MODE_BRIDGE
        case OperatingMode::MASTER:
            root_items[root_item_count++] = &item_color;
            root_items[root_item_count++] = &item_effect;
            root_items[root_item_count++] = &item_tube_fx;
            root_items[root_item_count++] = &item_tubes;
            root_items[root_item_count++] = &item_bpm;
            break;

        case OperatingMode::ARTNET:
            /* Addr and Tubes define the slice that is forwarded to the tubes, as in DMX mode.
               Pixels belonged to the Pixel Controller Bridge Mode and has no meaning without
               a local strip: root_items[root_item_count++] = &item_pixels; */
            root_items[root_item_count++] = &item_addr;
            root_items[root_item_count++] = &item_tubes;
            root_items[root_item_count++] = &item_dhcp;
            root_items[root_item_count++] = &menu_node_ip;
            root_items[root_item_count++] = &item_net;
            root_items[root_item_count++] = &item_subnet;
            root_items[root_item_count++] = &item_universe;
            break;
#elif DEVICE_MODE == DEVICE_MODE_TUBE
        case OperatingMode::STANDALONE:
            root_items[root_item_count++] = &item_color;
            root_items[root_item_count++] = &item_effect;
            break;
#endif
        case OperatingMode::DMX:
            root_items[root_item_count++] = &item_addr;
#if DEVICE_MODE == DEVICE_MODE_BRIDGE
            root_items[root_item_count++] = &item_tubes;
#endif  
            break;
    }

    current_menu_items = root_items;
    current_menu_count = root_item_count;
    selected_index = 0;
    is_in_edit_mode = false;
    menu_depth = 0;
    update_display = true;
}

/* ============= MENU HANDLING ============= */

/**
 * @brief Main menu handler called in loop
 * 
 * Updates display and processes autoincrement for held buttons.
 * 
 * @see show()
 * @see process_autoincrement()
 */
void Menu::handle() {
    show();
    process_scroll();
    process_autoincrement();
    process_pending_save();
}

/* ================= ISR ================= */

void Menu::handle_up_isr()    { if (instance) instance->handle_up(); }
void Menu::handle_down_isr()  { if (instance) instance->handle_down(); }
void Menu::handle_enter_isr() { if (instance) instance->handle_enter(); }

/* ================= INPUT ================= */

/**
 * @brief Checks if sufficient time has passed since last button press
 * 
 * Implements debounce logic to prevent multiple triggers from single press.
 * 
 * @return true if debounce time has elapsed, false otherwise
 */
bool Menu::check_debounce() {
    static unsigned long last_press = 0;
    if (millis() - last_press < BUTTON_DEBOUNCE) return false;
    last_press = millis();
    return true;
}

void Menu::handle_up() {
    if (!check_debounce()) return;

    button_press_time = millis();
    button_held = ButtonState::UP;

    if (is_in_edit_mode) {
        MenuItem* menu_item = get_current_item();
        menu_item->increment();
        mark_pending_save(menu_item->get_id());
    } else {
        selected_index = (selected_index + 1) % current_menu_count;
    }
    update_display = true;
}

/**
 * @brief Handles DOWN button press
 * 
 * In edit mode: decrements selected item and saves to flash.
 * In nav mode: navigates to previous menu item (wraps around).
 * Includes debounce and sets button_held for autoincrement detection.
 * 
 * @see handle_up()
 * @see process_autoincrement()
 */
void Menu::handle_down() {
    if (!check_debounce()) return;

    button_press_time = millis();
    button_held = ButtonState::DOWN;

    if (is_in_edit_mode) {
        MenuItem* menu_item = get_current_item();
        menu_item->decrement();
        mark_pending_save(menu_item->get_id());
    } else {
        selected_index = (selected_index + current_menu_count - 1) % current_menu_count;
    }
    update_display = true;
}

/**
 * @brief Handles ENTER button press
 * 
 * Toggles between navigation and edit mode.
 * If exiting edit mode with MODE selected, rebuilds visible menu for new mode.
 * Stops autoincrement by clearing button_held.
 * Includes debounce.
 * 
 * @see is_in_edit_mode
 * @see build_visible_menu()
 */
void Menu::handle_enter() {
    if (!check_debounce()) return;

    button_held = ButtonState::NONE;

    MenuItem* menu_item = get_current_item();

    // Check if clicking back item
    if (menu_item->is_back_item()) {
        navigate_back();
    }
    // Check if entering a submenu
    else if (!is_in_edit_mode && menu_item->is_node()) {
        navigate_to_menu_node(menu_item);
    }
    // Toggle edit mode for leaf items
    else if (!menu_item->is_node()) {
        is_in_edit_mode = !is_in_edit_mode;

        // If exiting edit mode and current item is Mode, rebuild menu
        if (!is_in_edit_mode && menu_item == &item_mode) {
            current_mode = static_cast<OperatingMode>(item_mode.get_value());
            build_menu_tree();
        }
    }
    update_display = true;
}

/* ================= NAVIGATION ================= */

/**
 * @brief Returns pointer to currently selected menu item
 * 
 * @return MenuItem* Pointer to current item in active menu
 */
MenuItem* Menu::get_current_item() {
    return current_menu_items[selected_index];
}

/**
 * @brief Navigates into a submenu node
 * 
 * Saves current menu state to stack, enters submenu, and resets selection.
 * Includes stack overflow protection.
 * 
 * @param menu_node Pointer to MenuNode to navigate into
 */
void Menu::navigate_to_menu_node(MenuItem* menu_node) {
    if (menu_depth >= MAX_MENU_DEPTH) return;  // Stack overflow protection

    // Save current state
    menu_stack[menu_depth] = current_menu_items;
    menu_count_stack[menu_depth] = current_menu_count;
    selected_index_stack[menu_depth] = selected_index;
    menu_depth++;

    // Enter menu node
    current_menu_items = menu_node->get_children();
    current_menu_count = menu_node->get_child_count();
    selected_index = 0;
    is_in_edit_mode = false;
}

/**
 * @brief Navigates back to parent menu
 * 
 * Restores menu state from stack and exits edit mode.
 * Does nothing if already at root level.
 */
void Menu::navigate_back() {
    if (menu_depth == 0) return;  // Already at root

    menu_depth--;
    current_menu_items = menu_stack[menu_depth];
    current_menu_count = menu_count_stack[menu_depth];
    selected_index = selected_index_stack[menu_depth];
    is_in_edit_mode = false;
}

/* ================= AUTOINCREMENT ================= */

/**
 * @brief Main autoinc handler for held Up/Down buttons
 * 
 * Triggers automatic value changes after 1 seconds hold time, repeating every 75ms.
 * Detects button release and executes repeat actions based on button_held state.
 */
void Menu::process_autoincrement() {
    update_button_release_state();
    
    if (button_held == ButtonState::NONE) return;
    
    if (should_trigger_increment()) {
        trigger_increment_action();
    }
}

/**
 * @brief Detects physical button release by reading pin states
 * 
 * Clears button_held when both pins return HIGH (INPUT_PULLUP mode).
 * Required since ISRs only detect FALLING edge, not RISING edge.
 */
void Menu::update_button_release_state() {
    if (digitalRead(PIN_UP) == HIGH && digitalRead(PIN_DOWN) == HIGH) {
        button_held = ButtonState::NONE;
    }
}

/**
 * @brief Checks if repeat timing conditions are met
 * 
 * Returns true when button held ≥ 2 seconds AND ≥ 75ms since last repeat.
 * 
 * @return true if autorepeat should trigger
 */
bool Menu::should_trigger_increment() {
    unsigned long now = millis();
    unsigned long hold_time = now - button_press_time;
    
    return (hold_time >= AUTOINCREMENT_DELAY && (now - last_autoincrement) >= AUTOINCREMENT_INTERVAL);
}

/**
 * @brief Executes increment/decrement or navigation action
 * 
 * Repeats the initial button press action:
 * - UP: Increment (edit mode) or navigate up (nav mode)
 * - DOWN: Decrement (edit mode) or navigate down (nav mode)
 * 
 * Saves to flash in edit mode.
 */
void Menu::trigger_increment_action() {
    last_autoincrement = millis();
    
    if (button_held == ButtonState::UP) {
        if (is_in_edit_mode) {
            get_current_item()->increment();
            mark_pending_save(get_current_item()->get_id());
        } else {
            selected_index = (selected_index + 1) % current_menu_count;
        }
    } else if (button_held == ButtonState::DOWN) {
        if (is_in_edit_mode) {
            get_current_item()->decrement();
            mark_pending_save(get_current_item()->get_id());
        } else {
            selected_index = (selected_index + current_menu_count - 1) % current_menu_count;
        }
    }
    update_display = true;
}

void Menu::mark_pending_save(MenuItemID id) {
    save_id = id;
    save_flag = true;
}

void Menu::process_pending_save() {
    if (!save_flag) return;

    noInterrupts();
    MenuItemID pending_id = save_id;
    save_flag = false;
    interrupts();

    save_to_flash(pending_id);
}

/* ================= DISPLAY ================= */

/**
 * @brief Renders current menu item to OLED display
 * 
 * Shows item name and value. Inverted colors in edit mode, normal colors in navigation mode.
 * Only updates display when update_display flag is set.
 */
void Menu::render_text() {
    display.clearDisplay();
    display.setCursor(-scroll_offset, 16);
    if (is_in_edit_mode) {
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    } else {
        display.setTextColor(SSD1306_WHITE);
    }
    display.print(scroll_text);
    display.display();
}

void Menu::show() {
    if (!update_display) return;

    MenuItem* item = get_current_item();
    String text = item->get_name();
    if (!item->is_node()) {
        text += ": ";
        text += item->get_display_value();
    }

    if (text != scroll_text) {
        scroll_text    = text;
        scroll_offset  = 0;
        scroll_next_ms = millis() + SCROLL_PAUSE;
        scroll_active  = (int16_t)(text.length() * CHAR_WIDTH) > SCREEN_WIDTH;
    }

    render_text();
    update_display = false;
}

void Menu::process_scroll() {
    if (!scroll_active) return;

    update_scroll_state();
    render_text();
}

void Menu::update_scroll_state() {
    unsigned long now = millis();
    if (now < scroll_next_ms) return;

    int16_t max_offset = (int16_t)(scroll_text.length() * CHAR_WIDTH) - SCREEN_WIDTH;

    if (scroll_offset > max_offset) {
        scroll_offset  = 0;
        scroll_next_ms = now + SCROLL_PAUSE;
    } else if (scroll_offset == max_offset) {
        scroll_offset  = max_offset + 1; // sentinel: pause elapsed, next tick resets
        scroll_next_ms = now + SCROLL_PAUSE;
    } else {
        scroll_offset += SCROLL_STEP;
        scroll_next_ms = now + SCROLL_INTERVAL;
    }
}

/* ================= FLASH MEMORY ================= */

/**
 * @brief Loads all menu item values from NVS flash storage
 * 
 * Restores previously saved values for each menu item.
 * Sets current_mode based on loaded MODE value.
 */
void Menu::load_from_flash() {
    preferences.begin("menu_cfg", false);
    for (uint8_t i = 0; i < MENU_ITEM_COUNT; i++) {
        MenuItem* menu_item = get_item((MenuItemID)i);
        if (!menu_item->is_node()) {
            String key = String(menu_item->get_name());
            uint16_t value = (uint16_t)preferences.getUInt(key.c_str(), menu_item->get_value());
            menu_item->set_value(value);
        }
    }
    preferences.end();
    current_mode = static_cast<OperatingMode>(item_mode.get_value());
    
}

/**
 * @brief Performs flash write
 * 
 * Writes the selected menu item value to NVS flash storage.
 */
void Menu::save_to_flash(MenuItemID id) {
    MenuItem* menu_item = get_item(id);
    if (menu_item->is_node()) return;
    
    preferences.begin("menu_cfg", false);
    String key = String(menu_item->get_name());
    preferences.putUInt(key.c_str(), menu_item->get_value());  
    preferences.end();
}

/* ================= GETTER ================= */

uint8_t Menu::get_mode()        { return item_mode.get_value(); }
uint8_t Menu::get_address()     { return item_addr.get_value(); }
CRGB    Menu::get_color()       { return is_color_auto() ? CRGB::Black : color_values[item_color.get_value()]; }
bool    Menu::is_color_auto()   { return item_color.get_value() == AUTO_VALUE; }
uint8_t Menu::get_effect()      { return item_effect.get_value(); }
bool    Menu::is_effect_auto()  { return item_effect.get_value() == AUTO_VALUE; }
uint8_t Menu::get_tube_effect() { return item_tube_fx.get_value(); }
uint8_t Menu::get_channel()     { return item_channel.get_value(); }
uint8_t Menu::get_tube_number() { return item_tubes.get_value(); }
uint8_t Menu::get_bpm()         { return item_bpm.get_value(); }
uint16_t Menu::get_pixels()      { return item_pixels.get_value(); }
uint8_t Menu::get_net()         { return item_net.get_value(); }
uint8_t Menu::get_subnet()      { return item_subnet.get_value(); }
uint8_t Menu::get_universe()    { return item_universe.get_value(); }

/**
 * @brief Gets MenuItem pointer by ID
 * 
 * @param id MenuItemID to look up
 * @return MenuItem* Pointer to menu item, or nullptr if not found
 */
MenuItem* Menu::get_item(MenuItemID id) {
    switch(id) {
        case ITEM_MODE:     return &item_mode;
        case ITEM_ADDR:     return &item_addr;
        case ITEM_COLOR:    return &item_color;
        case ITEM_EFFECT:   return &item_effect;
        case ITEM_TUBE_FX:  return &item_tube_fx;
        case ITEM_CHANNEL:  return &item_channel;
        case ITEM_TUBES:    return &item_tubes;
        case ITEM_BPM:      return &item_bpm;
        case ITEM_PIXELS:   return &item_pixels;
        case ITEM_DHCP:     return &item_dhcp;
        case ITEM_IP_ADDR:  return &menu_node_ip;
        case ITEM_IP_O1:    return &item_ip_o1;
        case ITEM_IP_O2:    return &item_ip_o2;
        case ITEM_IP_O3:    return &item_ip_o3;
        case ITEM_IP_O4:    return &item_ip_o4;
        case ITEM_NET:      return &item_net;
        case ITEM_SUBNET:   return &item_subnet;
        case ITEM_UNIVERSE: return &item_universe;
        case MENU_ITEM_BACK: return &back_item;
        default:            return nullptr;
    }
}