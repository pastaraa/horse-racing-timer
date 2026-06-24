#include "p10_display.h"
#include "p10_config.h"
#include "p10_font.h"
#include "p10_logo.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_log.h"

static const char *TAG = "P10_DISPLAY";

// Framebuffer global 
static uint8_t framebuffer[TOTAL_HEIGHT][FB_ROW_BYTES];
static spi_device_handle_t spi;

// ─── PRIVATE FUNCTIONS ───────────────────────────────────────────
static void p10_set_pixel(int x, int y, uint8_t on) {
    if (x < 0 || x >= TOTAL_WIDTH || y < 0 || y >= TOTAL_HEIGHT) return;
    int byte_idx = x / 8;
    int bit_idx  = 7 - (x % 8);
    if (on) framebuffer[y][byte_idx] &= ~(1 << bit_idx);
    else    framebuffer[y][byte_idx] |=  (1 << bit_idx);
}

static const uint8_t* font_find(char c) {
    for (int i = 0; i < FONT_TABLE_SIZE; i++) {
        if (font_table[i].ch == c) return font_table[i].rows;
    }
    return NULL;
}

static void p10_draw_char(int x_offset, int y_offset, char c) {
    const uint8_t *glyph = font_find(c);
    if (glyph == NULL) return;
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            uint8_t on = (glyph[row] >> (4 - col)) & 1;
            p10_set_pixel(x_offset + col, y_offset + row, on);
        }
    }
}

// ─── TASK & HARDWARE CONTROL (f. DRAW_P10) ───────────────────────
static void p10_send_row(uint8_t scan_row) {
    uint8_t scan_buffer[48] = {0};
    int idx = 0;
    for (int p = 0; p < PANELS_X; p++) {
        for (int c = 0; c < 4; c++) {
            int byte_idx = p * 4 + c;
            scan_buffer[idx++] = framebuffer[scan_row + 12][byte_idx];
            scan_buffer[idx++] = framebuffer[scan_row +  8][byte_idx];
            scan_buffer[idx++] = framebuffer[scan_row +  4][byte_idx];
            scan_buffer[idx++] = framebuffer[scan_row     ][byte_idx];
        }
    }
    gpio_set_level(PIN_OE, 1);
    spi_transaction_t t = {
        .length    = sizeof(scan_buffer) * 8,
        .tx_buffer = scan_buffer,
    };
    spi_device_transmit(spi, &t);
    gpio_set_level(PIN_A,   (scan_row >> 0) & 1);
    gpio_set_level(PIN_B,   (scan_row >> 1) & 1);
    gpio_set_level(PIN_LAT, 1);
    gpio_set_level(PIN_LAT, 0);
    gpio_set_level(PIN_OE,  0);
}

static void p10_refresh_task(void *pvParameter) {
    uint8_t scan_row = 0;
    while (1) {
        p10_send_row(scan_row);
        scan_row = (scan_row + 1) % SCAN_ROWS;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ─── PUBLIC FUNCTIONS ────────────────────────────────────────────
void p10_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_OE) | (1ULL << PIN_A) |
                        (1ULL << PIN_B)  | (1ULL << PIN_LAT),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(PIN_OE,  1);
    gpio_set_level(PIN_A,   0);
    gpio_set_level(PIN_B,   0);
    gpio_set_level(PIN_LAT, 0);

    spi_bus_config_t bus_cfg = {
        .mosi_io_num   = PIN_R,
        .miso_io_num   = -1,
        .sclk_io_num   = PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_bus_initialize(VSPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 1000000,
        .mode           = 0,
        .spics_io_num   = -1,
        .queue_size     = 1,
    };
    spi_bus_add_device(VSPI_HOST, &dev_cfg, &spi);

    xTaskCreatePinnedToCore(p10_refresh_task, "p10_refresh", 4096, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "P10 Display initialized");
}

void p10_clear(void) {
    memset(framebuffer, 0xff, sizeof(framebuffer));
}

void p10_draw_logo(void) {
    for (int y = 0; y < TOTAL_HEIGHT; y++) {
        for (int x = 0; x < FB_ROW_BYTES; x++) {
            framebuffer[y][x] = ~logo_bitmap[y][x];
        }
    }
}

void p10_string_to_buffer(const char *str, int x, int y, p10_align_h_t align_h, p10_align_v_t align_v) {
    int len = strlen(str);
    int text_width = (len * 6) - 1;
    int text_height = 7;

    int final_x = x;
    int final_y = y;

    if (align_h == ALIGN_CENTER) final_x = (TOTAL_WIDTH - text_width) / 2;
    else if (align_h == ALIGN_RIGHT) final_x = TOTAL_WIDTH - text_width - x;

    if (align_v == ALIGN_MIDDLE) final_y = (TOTAL_HEIGHT - text_height) / 2;
    else if (align_v == ALIGN_BOTTOM) final_y = TOTAL_HEIGHT - text_height - y;

    int curr_x = final_x;
    while (*str) {
        p10_draw_char(curr_x, final_y, *str);
        curr_x += 6;
        str++;
    }
}

void p10_int_to_buffer(int angka, int x, int y, p10_align_h_t align_h, p10_align_v_t align_v) {
    char str_buff[16];
    snprintf(str_buff, sizeof(str_buff), "%d", angka);
    p10_string_to_buffer(str_buff, x, y, align_h, align_v);
}

void p10_timestamp_to_buffer(long timestamp, int x, int y, p10_align_h_t align_h, p10_align_v_t align_v) {
    char str_buff[32];
    snprintf(str_buff, sizeof(str_buff), "%ld", timestamp);
    p10_string_to_buffer(str_buff, x, y, align_h, align_v);
}