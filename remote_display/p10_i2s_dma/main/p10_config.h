#ifndef P10_CONFIG_H
#define P10_CONFIG_H

#include "driver/gpio.h"

// ─── PIN & PANEL CONFIG ──────────────────────────────────────────
#define PIN_R   GPIO_NUM_23
#define PIN_CLK GPIO_NUM_19
#define PIN_LAT GPIO_NUM_18
#define PIN_OE  GPIO_NUM_21
#define PIN_A   GPIO_NUM_16
#define PIN_B   GPIO_NUM_17

#define PANEL_WIDTH    32
#define PANEL_HEIGHT   16
#define PANELS_X       3
#define TOTAL_WIDTH    (PANEL_WIDTH * PANELS_X)
#define TOTAL_HEIGHT   PANEL_HEIGHT
#define SCAN_ROWS      4
#define FB_ROW_BYTES   (TOTAL_WIDTH / 8)

#endif // P10_CONFIG_H