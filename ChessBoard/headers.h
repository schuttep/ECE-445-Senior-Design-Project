#ifndef HEADERS_H
#define HEADERS_H

#include "secrets.h"

// Common RGB565 colour values — guarded so the DFRobot GDL library
// can also define them without a redefinition error.
#ifndef COLOR_RGB565_BLACK
#define COLOR_RGB565_BLACK 0x0000
#define COLOR_RGB565_WHITE 0xFFFF
#define COLOR_RGB565_RED 0xF800
#define COLOR_RGB565_GREEN 0x07E0
#define COLOR_RGB565_BLUE 0x001F
#define COLOR_RGB565_YELLOW 0xFFE0
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