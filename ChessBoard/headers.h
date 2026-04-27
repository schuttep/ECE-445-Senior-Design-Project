#ifndef HEADERS_H
#define HEADERS_H

#include "secrets.h"

// Standard RGB565 colours — guarded so the DFRobot GDL library can also define
// them without a redefinition error.
#ifndef COLOR_RGB565_BLACK
#define COLOR_RGB565_BLACK 0x0000
#define COLOR_RGB565_WHITE 0xFFFF
#define COLOR_RGB565_RED 0xF800
#define COLOR_RGB565_GREEN 0x07E0
#define COLOR_RGB565_BLUE 0x001F
#define COLOR_RGB565_YELLOW 0xFFE0
#endif

// Extended palette — defined outside the library guard so these are always
// available even when the DFRobot library has already defined the standard set.
// Each entry uses its own guard to avoid redefinition if a future library
// version uses the same name.
#ifndef COLOR_RGB565_ORANGE
#define COLOR_RGB565_ORANGE 0xFD20 // piece-lift banner, orange accents
#endif
#ifndef COLOR_RGB565_DARK_NAVY
#define COLOR_RGB565_DARK_NAVY 0x2945 // edge-case header, screen backgrounds
#endif
#ifndef COLOR_RGB565_DARK_GREEN
#define COLOR_RGB565_DARK_GREEN 0x0460 // confirmed-move status bar
#endif
#ifndef COLOR_RGB565_DARK_GREY
#define COLOR_RGB565_DARK_GREY 0x4208 // back button, disabled states
#endif
#ifndef COLOR_RGB565_MID_GREY
#define COLOR_RGB565_MID_GREY 0xC618 // borders, dividers
#endif
#ifndef COLOR_RGB565_PALE_GREY
#define COLOR_RGB565_PALE_GREY 0x7BEF // light borders, scroll arrows
#endif
#ifndef COLOR_RGB565_LIGHT_GREY
#define COLOR_RGB565_LIGHT_GREY 0xEF7D // input field backgrounds
#endif
#ifndef COLOR_RGB565_CYAN
#define COLOR_RGB565_CYAN 0x07FF // highlight text
#endif
#ifndef COLOR_RGB565_OLIVE
#define COLOR_RGB565_OLIVE 0x630C // secondary / muted text
#endif
#ifndef COLOR_RGB565_DARK_PURPLE
#define COLOR_RGB565_DARK_PURPLE 0x18C3 // promotion picker background
#endif
#ifndef COLOR_RGB565_DEEP_RED
#define COLOR_RGB565_DEEP_RED 0xF000 // board sync extra-piece tint
#endif
#ifndef COLOR_RGB565_DIM_RED
#define COLOR_RGB565_DIM_RED 0xF810 // board sync missing-piece text
#endif
#ifndef COLOR_RGB565_GOLD
#define COLOR_RGB565_GOLD 0xFEA0 // promotion Queen button
#endif
#ifndef COLOR_RGB565_KB_KEY
#define COLOR_RGB565_KB_KEY 0x8410 // on-screen keyboard key background
#endif
#ifndef COLOR_RGB565_PALE_PINK
#define COLOR_RGB565_PALE_PINK 0xF7BE // WiFi list alternate row
#endif

#define SDA_DAQ 38
#define SCL_DAQ 39
#define SCR_SCLK 45
#define SCR_MOSI 48
#define SCR_MISO 47
#define SCR_CS 21
#define SCR_RST 14
#define SCR_DC 13
#define SCR_BLK 12
#define SCR_I2C_SCL 11
#define SCR_I2C_SDA 10
#define SCR_INT 9
#define SCR_TCH_RST 8

#endif