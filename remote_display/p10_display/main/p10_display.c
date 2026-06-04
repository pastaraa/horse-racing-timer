#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"

static const char *TAG = "P10_DISPLAY";

// ─── Pin Definitions ────────────────────────────────────────────
#define PIN_OE      GPIO_NUM_16
#define PIN_A       GPIO_NUM_19
#define PIN_B       GPIO_NUM_21
#define PIN_LAT     GPIO_NUM_22
#define PIN_CLK     GPIO_NUM_18
#define PIN_DATA    GPIO_NUM_23

// ─── Panel Configuration ─────────────────────────────────────────
#define PANEL_WIDTH     32
#define PANEL_HEIGHT    16
#define PANELS_X        6
#define PANELS_Y        3
#define TOTAL_WIDTH     (PANEL_WIDTH  * PANELS_X)
#define TOTAL_HEIGHT    (PANEL_HEIGHT * PANELS_Y)
#define SCAN_ROWS       4

// ─── Font 8x16 ───────────────────────────────────────────────────
static const uint8_t font8x16[][16] = {
    { 0x3C,0x66,0x66,0x6E,0x76,0x66,0x66,0x3C,0x3C,0x66,0x66,0x6E,0x76,0x66,0x66,0x3C }, // 0
    { 0x18,0x38,0x18,0x18,0x18,0x18,0x18,0x7E,0x18,0x38,0x18,0x18,0x18,0x18,0x18,0x7E }, // 1
    { 0x3C,0x66,0x06,0x0C,0x18,0x30,0x60,0x7E,0x3C,0x66,0x06,0x0C,0x18,0x30,0x60,0x7E }, // 2
    { 0x3C,0x66,0x06,0x1C,0x06,0x06,0x66,0x3C,0x3C,0x66,0x06,0x1C,0x06,0x06,0x66,0x3C }, // 3
    { 0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x0C,0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x0C }, // 4
    { 0x7E,0x60,0x60,0x7C,0x06,0x06,0x66,0x3C,0x7E,0x60,0x60,0x7C,0x06,0x06,0x66,0x3C }, // 5
    { 0x1C,0x30,0x60,0x7C,0x66,0x66,0x66,0x3C,0x1C,0x30,0x60,0x7C,0x66,0x66,0x66,0x3C }, // 6
    { 0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x30,0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x30 }, // 7
    { 0x3C,0x66,0x66,0x3C,0x66,0x66,0x66,0x3C,0x3C,0x66,0x66,0x3C,0x66,0x66,0x66,0x3C }, // 8
    { 0x3C,0x66,0x66,0x3E,0x06,0x06,0x0C,0x38,0x3C,0x66,0x66,0x3E,0x06,0x06,0x0C,0x38 }, // 9
    { 0x00,0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00,0x00,0x18,0x18 }, // :
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18 }, // .
};
#define FONT_IDX_COLON  10
#define FONT_IDX_DOT    11

// ─── Framebuffer ─────────────────────────────────────────────────
#define FB_ROW_BYTES    (TOTAL_WIDTH / 8)
static uint8_t framebuffer[TOTAL_HEIGHT][FB_ROW_BYTES];

// ─── SPI Handle ──────────────────────────────────────────────────
static spi_device_handle_t spi;

// ─── Inisialisasi GPIO ───────────────────────────────────────────
static void p10_gpio_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_OE)  |
                        (1ULL << PIN_A)   |
                        (1ULL << PIN_B)   |
                        (1ULL << PIN_LAT),
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
    ESP_LOGI(TAG, "GPIO initialized");
}

// ─── Inisialisasi SPI ────────────────────────────────────────────
static void p10_spi_init(void)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num   = PIN_DATA,
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
    ESP_LOGI(TAG, "SPI initialized");
}

// ─── Set / Clear satu pixel ──────────────────────────────────────
static void p10_set_pixel(int x, int y, uint8_t on)
{
    if (x < 0 || x >= TOTAL_WIDTH || y < 0 || y >= TOTAL_HEIGHT) return;
    int byte_idx = x / 8;
    int bit_idx  = 7 - (x % 8);
    if (on) {
        framebuffer[y][byte_idx] |=  (1 << bit_idx);
    } else {
        framebuffer[y][byte_idx] &= ~(1 << bit_idx);
    }
}

// ─── Clear seluruh display ───────────────────────────────────────
static void p10_clear(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
}

// ─── Draw satu karakter ──────────────────────────────────────────
static void p10_draw_char(char ch, int x, int y)
{
    int idx;
    if      (ch >= '0' && ch <= '9') idx = ch - '0';
    else if (ch == ':')              idx = FONT_IDX_COLON;
    else if (ch == '.')              idx = FONT_IDX_DOT;
    else return;

    for (int row = 0; row < 16; row++) {
        uint8_t bits = font8x16[idx][row];
        for (int col = 0; col < 8; col++) {
            uint8_t pixel_on = (bits >> (7 - col)) & 1;
            p10_set_pixel(x + col, y + row, pixel_on);
        }
    }
}

// ─── Draw string ─────────────────────────────────────────────────
static void p10_draw_string(const char *str, int x, int y)
{
    while (*str) {
        p10_draw_char(*str, x, y);
        x += 9;
        str++;
    }
}

// ─── Kirim satu scan row ─────────────────────────────────────────
static void p10_send_row(uint8_t scan_row)
{
    uint8_t row_data[FB_ROW_BYTES];
    memcpy(row_data, framebuffer[scan_row], FB_ROW_BYTES);

    gpio_set_level(PIN_OE, 1);

    spi_transaction_t t = {
        .length    = FB_ROW_BYTES * 8,
        .tx_buffer = row_data,
    };
    spi_device_transmit(spi, &t);

    gpio_set_level(PIN_A, (scan_row >> 0) & 1);
    gpio_set_level(PIN_B, (scan_row >> 1) & 1);
    gpio_set_level(PIN_LAT, 1);
    gpio_set_level(PIN_LAT, 0);
    gpio_set_level(PIN_OE, 0);
}

// ─── Refresh Task ────────────────────────────────────────────────
static void p10_refresh_task(void *pvParameter)
{
    uint8_t scan_row = 0;
    while (1) {
        p10_send_row(scan_row);
        scan_row = (scan_row + 1) % SCAN_ROWS;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ─── App Main ────────────────────────────────────────────────────
void app_main(void)
{
    ESP_LOGI(TAG, "P10 Display starting...");
    p10_clear();
    p10_gpio_init();
    p10_spi_init();

    xTaskCreatePinnedToCore(
        p10_refresh_task,
        "p10_refresh",
        4096,
        NULL,
        5,
        NULL,
        1
    );

    // Test: tampilkan timer dummy
    p10_draw_string("01:23:456", 0, 0);

    ESP_LOGI(TAG, "Display running! %dx%d px", TOTAL_WIDTH, TOTAL_HEIGHT);
}