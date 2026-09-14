#ifndef MENU_ITEM_HPP
#define MENU_ITEM_HPP

#include <Arduino.h>
#include <FastLED.h>
#include <functional>


struct MenuOption {
    const char* name;
    uint8_t value;
};

enum MenuItemID {
    ITEM_MODE,
    ITEM_ADDR,
    ITEM_COLOR,
    ITEM_EFFECT,
    ITEM_TUBE_FX,
    ITEM_CHANNEL,
    ITEM_TUBES,
    ITEM_BPM,
    ITEM_PIXELS,
    ITEM_DHCP,
    ITEM_IP_ADDR,
    ITEM_IP_O1,
    ITEM_IP_O2,
    ITEM_IP_O3,
    ITEM_IP_O4,
    ITEM_NET,
    ITEM_SUBNET,
    ITEM_UNIVERSE,
    MENU_ITEM_BACK,
    MENU_ITEM_COUNT
};


class MenuItem {
public:

    MenuItem(MenuItemID id, const char* name) : id(id), name(name) {}
    virtual ~MenuItem() {}

    // Leaf item operations
    virtual void increment() {}
    virtual void decrement() {}
    virtual String get_display_value() { return ""; }
    virtual uint16_t get_value() { return 0; }
    virtual void set_value(uint16_t value) {}
    virtual MenuItemID get_id() { return id; }

    // Node operations
    virtual MenuItem** get_children() { return nullptr; }
    virtual uint8_t get_child_count() { return 0; }
    virtual bool is_node() const { return false; }
    
    // Display name (can be overridden for dynamic names)
    virtual String get_name() { return name; }
    
    // Check if this is a back item
    virtual bool is_back_item() const { return false; }

protected:
    MenuItemID id;
    const char* name;
    uint16_t current_value;
};


class NumericMenuItem : public MenuItem {
public:
    NumericMenuItem(MenuItemID id, const char* name, uint16_t initial_value, uint16_t min, uint16_t max)
        : MenuItem(id, name), min_value(min), max_value(max) {
        current_value = constrain(initial_value, min, max);
    }

    void increment() override {
        if (current_value < max_value) current_value++;
        else current_value = min_value;     //overflow to min
    }

    void decrement() override {
        if (current_value > min_value) current_value--;
        else current_value = max_value;     //overflow to max
    }

    String get_display_value() override { return String(current_value); }
    uint16_t get_value() override { return current_value; }
    void set_value(uint16_t value) override { current_value = constrain(value, min_value, max_value); }

private:
    uint16_t min_value;
    uint16_t max_value;
};

class CategoryMenuItem : public MenuItem {
public:
    CategoryMenuItem(MenuItemID id, const char* name, uint8_t initial_value, const MenuOption* options, uint8_t num_options)
        : MenuItem(id, name), options(options), num_options(num_options) {
        set_value(initial_value);
    }

    void increment() override { current_index = (current_index + 1) % num_options; }
    void decrement() override { current_index = (current_index - 1 + num_options) % num_options; }
    String get_display_value() override { return options[current_index].name; }
    uint16_t get_value() override { return options[current_index].value; }
    
    void set_value(uint16_t value) override {
        for (uint8_t i = 0; i < num_options; ++i) {
            if (options[i].value == value) {
                current_index = i;
                current_value = value;
                return;
            }
        }
        current_index = 0;
        current_value = options[0].value;
    }

private:
    const MenuOption* options;
    uint8_t num_options;
    uint8_t current_index;
};


class MenuNode : public MenuItem {
public:
    MenuNode(MenuItemID id, const char* name, MenuItem** children, uint8_t child_count,
             std::function<String()> name_callback = nullptr)
        : MenuItem(id, name), children(children), child_count(child_count),
          name_callback(name_callback) {}

    MenuItem** get_children() override { return children; }
    uint8_t get_child_count() override { return child_count; }
    bool is_node() const override { return true; }
    
    String get_name() override {
        return name_callback ? name_callback() : name;
    }

private:
    MenuItem** children;
    uint8_t child_count;
    std::function<String()> name_callback;
};


class BackMenuItem : public MenuItem {
public:
    BackMenuItem() : MenuItem(MENU_ITEM_BACK, "< Back") {}

    bool is_back_item() const { return true; }
};

#endif