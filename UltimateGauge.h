#include <Arduino.h>
#include <lvgl.h>
#include <stdio.h>

// Color palette
lv_color_t PALETTE_BLACK        = LV_COLOR_MAKE(0, 0, 0);
lv_color_t PALETTE_WHITE        = LV_COLOR_MAKE(255, 255, 255);
lv_color_t PALETTE_GREY         = LV_COLOR_MAKE(220, 220, 220);
lv_color_t PALETTE_DARK_GREY    = LV_COLOR_MAKE(160, 160, 160);
lv_color_t PALETTE_AMBER        = LV_COLOR_MAKE(250, 140, 0);
lv_color_t PALETTE_RED          = LV_COLOR_MAKE(220, 0, 0);
lv_color_t PALETTE_GREEN        = LV_COLOR_MAKE(0, 220, 0);

// UI design attributes
const int BACKLIGHT_INTRO_TIME  = 3000; // initial screen fade in
const int TRANSITION_FADE_TIME  = 1000; // screen transition (each direction, out and in) - total is 2x value + 200ms
const int NOTIFIER_SPIN_TIME    = 6000; // how long a spinner shows


//////////////// There should be no need to ////////////////
//////////////// change any of the values   ////////////////
//////////////// past this point            ////////////////

// Meter parts
typedef struct struct_icon_parts {
    float min;              // Min range value
    float max;              // Max range value
    float alert;            // What value to set alert (-1 if unused)
    float warning;          // What value to set warning  (-1 if unused)
    bool flag_when;         // is state applied ABOVE or BELOW alert / warning value?
    char unit[4];           // eg V, psi, °C
  } struct_icon_parts;

// Tracking attributes
uint8_t dimmer_lv;   // dimmed level from 0 - 9
uint8_t current_brightness; // follows the current 

// Data from the buttons
typedef struct struct_buttons {
  uint8_t flag;
  uint8_t button;
  uint8_t press_type;
} struct_buttons;

// Data for channel switching
typedef struct struct_set_channel {
  uint8_t flag;
  uint8_t channel_id;
} struct_set_channel;

// IDs for gauges displays
#define GAUGE_SMALL_SPEEDO     0
#define GAUGE_SMALL_LEVELS     1
#define GAUGE_SMALL_LOCATION   2

// ESPNow data sources
#define FLAG_CANBUS             0
#define FLAG_GPS                1
#define FLAG_BUTTONS            2
#define FLAG_OIL_PRESSURE       3
#define FLAG_STARTUP            4
#define FLAG_SET_CHANNEL        5
#define FLAG_FUEL               6
#define FLAG_ONLINE             7

// Console button IDs
#define BUTTON_SETTING          0
#define BUTTON_MODE             1
#define BUTTON_BRIGHTNESS_UP    2
#define BUTTON_BRIGHTNESS_DOWN  3

// Button events
#define CLICK_EVENT_CLICK       0
#define CLICK_EVENT_DOUBLE      1
#define CLICK_EVENT_HOLD        2

// Greater or less than for alerts
// bool ABOVE = true;
// bool BELOW = false;

// // Compare current values to alert / warning ranges and return color
// lv_color_t get_state_color(struct_icon_parts obj, float value, bool is_icon) {
//   // check for -1 initialisation values and return default
//   if (value == -1) {
//     return (is_show_num || !is_icon) ? PALETTE_WHITE : PALETTE_GREY;
//   }
//   // check is passed value flags an alert
//   if (obj.flag_when == ABOVE) {
//     // Check warning first, if defined
//     if (obj.warning >= 0 && value > obj.warning) {
//         return PALETTE_RED;
//     }
//     // Check alert if defined
//     if (obj.alert >= 0 && value > obj.alert) {
//         return PALETTE_AMBER;
//     }
//   } else if (obj.flag_when == BELOW) {
//     // Check warning first, if defined
//     if (obj.warning >= 0 && value < obj.warning) {
//         return PALETTE_RED;
//     }
//     // Check alert if defined
//     if (obj.alert >= 0 && value < obj.alert) {
//         return PALETTE_AMBER;
//     }
//   }

//   // If number showing or not an icon return default white
//   if (is_show_num || !is_icon) {
//     return PALETTE_WHITE;
//   }
  
//   // Otherwise, return grey
//   return PALETTE_GREY;
// }