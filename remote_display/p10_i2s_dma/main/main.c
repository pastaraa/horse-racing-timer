#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"

static const char *TAG = "P10_DISPLAY";

#define PIN_R   GPIO_NUM_23
#define PIN_CLK GPIO_NUM_19
#define PIN_LAT GPIO_NUM_18
#define PIN_OE  GPIO_NUM_21
#define PIN_A   GPIO_NUM_16
#define PIN_B   GPIO_NUM_17

#define PANEL_WIDTH     32
#define PANEL_HEIGHT    16
#define PANELS_X        3
#define TOTAL_WIDTH     (PANEL_WIDTH * PANELS_X)
#define TOTAL_HEIGHT    PANEL_HEIGHT
#define SCAN_ROWS       4
#define FB_ROW_BYTES    (TOTAL_WIDTH / 8)

static uint8_t framebuffer[TOTAL_HEIGHT][FB_ROW_BYTES];
static spi_device_handle_t spi;

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

static void p10_spi_init(void)
{
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
    ESP_LOGI(TAG, "SPI initialized");
}

static void p10_set_pixel(int x, int y, uint8_t on)
{
    if (x < 0 || x >= TOTAL_WIDTH || y < 0 || y >= TOTAL_HEIGHT) return;
    int byte_idx = x / 8;
    int bit_idx  = 7 - (x % 8);
    if (on)
        framebuffer[y][byte_idx] &= ~(1 << bit_idx);
    else
        framebuffer[y][byte_idx] |=  (1 << bit_idx);
}

static void p10_clear(void)
{
    memset(framebuffer, 0xff, sizeof(framebuffer));
}

static void p10_send_row(uint8_t scan_row)
{
    uint8_t scan_buffer[48] = {0};
    int idx = 0;

    for (int p = PANELS_X - 1; p >= 0; p--) {
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

static void p10_refresh_task(void *pvParameter)
{
    uint8_t scan_row = 0;
    while (1) {
        p10_send_row(scan_row);
        scan_row = (scan_row + 1) % SCAN_ROWS;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "P10 Display starting...");

    p10_gpio_init();
    p10_spi_init();
    p10_clear();

    p10_set_pixel(47, 7, 1);
    p10_set_pixel(48, 7, 1);
    p10_set_pixel(47, 8, 1);
    p10_set_pixel(48, 8, 1);

    ESP_LOGI(TAG, "Test: 4 center pixels ON");

    xTaskCreatePinnedToCore(p10_refresh_task, "p10_refresh", 4096, NULL, 5, NULL, 1);
}