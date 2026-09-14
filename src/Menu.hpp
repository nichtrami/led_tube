#ifndef MENU_HPP
#define MENU_HPP

#include <Preferences.h>
#include <FastLED.h>
#include "MenuItem.hpp"
#include "Config.hpp"

#define BUTTON_DEBOUNCE 200
#define MAX_VISIBLE_ITEMS 6

enum class OperatingMode : uint8_t {
#if DEVICE_MODE == DEVICE_MODE_BRIDGE
    MASTER = 0,
    DMX = 1,
    ARTNET = 2
#elif DEVICE_MODE == DEVICE_MODE_TUBE
    STANDALONE = 0,
    DMX = 1,
#endif
};

enum class ButtonState : uint8_t {
    NONE = 0,
    UP = 1,
    DOWN = 2
};

class Menu {
public:
    Menu();
    Menu(const Menu&) = delete;
    Menu& operator=(const Menu&) = delete;
    Menu(Menu&&) = delete;
    Menu& operator=(Menu&&) = delete;

    void begin();
    void handle();

    // ===== Getter =====
    uint8_t get_mode();
    uint8_t get_address();
    CRGB get_color();
    bool is_color_auto();
    uint8_t get_effect();
    bool is_effect_auto();
    uint8_t get_tube_effect();
    uint8_t get_channel();
    uint8_t get_tube_number();
    uint8_t get_bpm();
    uint16_t get_pixels();
    uint8_t get_net();
    uint8_t get_subnet();
    uint8_t get_universe();

private:
    // ===== ISR =====
    static void handle_up_isr();
    static void handle_down_isr();
    static void handle_enter_isr();

    void handle_up();
    void handle_down();
    void handle_enter();
    bool check_debounce();
    
    void process_autoincrement();
    void process_pending_save();
    void mark_pending_save(MenuItemID id);
    void update_button_release_state();
    bool should_trigger_increment();
    void trigger_increment_action();
    
    /** @brief Renders the current menu item. Rebuilds scroll state if text changed. Skips if update_display is not set. */
    void show();
    /** @brief Builds the menu tree based on the current operating mode and resets navigation state. */
    void build_menu_tree();
    /** @brief Pushes current menu state onto the stack and enters the given submenu node. */
    void navigate_to_menu_node(MenuItem* menu_node);
    /** @brief Pops the menu stack and returns to the parent menu. No-op at root level. */
    void navigate_back();
    /** @brief Returns a pointer to the currently selected menu item. */
    MenuItem* get_current_item();

    void load_from_flash();
    void save_to_flash(MenuItemID id);
    MenuItem* get_item(MenuItemID id);


    Preferences preferences;

    // ===== Menu Items (Leaves) =====
    CategoryMenuItem item_mode;
    NumericMenuItem item_addr;
    CategoryMenuItem item_color;
    CategoryMenuItem item_effect;
    CategoryMenuItem item_tube_fx;
    NumericMenuItem item_channel;
    NumericMenuItem item_tubes;
    NumericMenuItem item_bpm;
    NumericMenuItem item_pixels;
    CategoryMenuItem item_dhcp;
    NumericMenuItem item_ip_o1;
    NumericMenuItem item_ip_o2;
    NumericMenuItem item_ip_o3;
    NumericMenuItem item_ip_o4;
    NumericMenuItem item_net;
    NumericMenuItem item_subnet;
    NumericMenuItem item_universe;

    BackMenuItem back_item; // Back menu item for menu nodes

    MenuNode menu_node_ip; // IP Address menu node

    // Root menu items
    MenuItem* root_items[MENU_ITEM_COUNT];
    uint8_t root_item_count;

    // Navigation stack for menu nodes
    static constexpr uint8_t MAX_MENU_DEPTH = 3;
    MenuItem** menu_stack[MAX_MENU_DEPTH];
    uint8_t menu_count_stack[MAX_MENU_DEPTH];
    uint8_t selected_index_stack[MAX_MENU_DEPTH];
    uint8_t menu_depth = 0;

    // Current navigation state
    MenuItem** current_menu_items;
    uint8_t current_menu_count;
    uint8_t selected_index = 0;
    bool is_in_edit_mode = false;

    OperatingMode current_mode;

    volatile bool update_display = false;
    volatile bool save_flag = false;
    volatile MenuItemID save_id;

    // ===== Autoincrement =====
    static constexpr unsigned long AUTOINCREMENT_DELAY = 1000;
    static constexpr unsigned long AUTOINCREMENT_INTERVAL = 50;

    volatile unsigned long button_press_time = 0;
    volatile ButtonState button_held = ButtonState::NONE;
    volatile unsigned long last_autoincrement = 0;

    // ===== Scroll =====
    /** @brief Drives the marquee animation. No-op if scroll_active is false. Called every loop via handle(). */
    void process_scroll();
    /**
     * @brief Advances scroll_offset by one tick if the interval has elapsed.
     *
     * Pauses SCROLL_PAUSE ms at start and end. Uses a sentinel value (max_offset + 1)
     * to distinguish "end pause" from "reset" across two separate ticks.
     */
    void update_scroll_state();
    /** @brief Renders scroll_text at the current scroll_offset. Inverted colors in edit mode. */
    void render_text();
    static constexpr uint8_t  CHAR_WIDTH      = 12;   // px per char at textsize 2
    static constexpr uint8_t  SCROLL_STEP     = 2;    // px per tick
    static constexpr uint8_t  SCROLL_INTERVAL = 50;   // ms per tick
    static constexpr uint16_t SCROLL_PAUSE    = 1500; // ms pause at start/end
    String scroll_text;
    int16_t scroll_offset  = 0;
    unsigned long scroll_next_ms = 0;
    bool scroll_active = false;

    static Menu* instance;
};

#endif