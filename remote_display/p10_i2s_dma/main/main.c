#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_now.h"

static const char *TAG = "P10_DISPLAY";

// ─── PIN & PANEL CONFIG ──────────────────────────────────────────
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

#define BRIGHTNESS_US   200

// ─── ESP-NOW PACKET ──────────────────────────────────────────────
#define CMD_PREPARE   0x47
#define CMD_START     0x51
#define CMD_FAULT     0x48
#define CMD_REFUSAL   0x49
#define CMD_ELIMINATE 0x4A
#define CMD_PHASE1    0x52
#define CMD_FINISH    0x53
#define CMD_HEARTBEAT 0x63

typedef struct {
    uint8_t  ver;
    uint8_t  cmd;
    uint8_t  p1Mode;
    uint8_t  p2Mode;
    uint8_t  p1Time;
    uint8_t  p2Time;
    uint32_t timeStamp;
} __attribute__((packed)) espnow_msg_t;

// ─── FRAMEBUFFER & SPI ───────────────────────────────────────────
static uint8_t framebuffer[TOTAL_HEIGHT][FB_ROW_BYTES];
static spi_device_handle_t spi;

// ─── FONT TABLE ──────────────────────────────────────────────────
typedef struct {
    char ch;
    uint8_t rows[7];
} font_glyph_t;

static const font_glyph_t font_table[] = {
    {'A', {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'B', {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
    {'C', {0x0F,0x10,0x10,0x10,0x10,0x10,0x0F}},
    {'D', {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}},
    {'E', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}},
    {'F', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
    {'G', {0x0F,0x10,0x10,0x17,0x11,0x11,0x0F}},
    {'H', {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'I', {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}},
    {'J', {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}},
    {'K', {0x11,0x12,0x14,0x18,0x14,0x12,0x11}},
    {'L', {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},
    {'M', {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}},
    {'N', {0x11,0x19,0x15,0x15,0x13,0x13,0x11}},
    {'O', {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}},
    {'P', {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
    {'Q', {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}},
    {'R', {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
    {'S', {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}},
    {'T', {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
    {'U', {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}},
    {'V', {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}},
    {'W', {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}},
    {'X', {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}},
    {'Y', {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}},
    {'Z', {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
    {'0', {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}},
    {'1', {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
    {'2', {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}},
    {'3', {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}},
    {'4', {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}},
    {'5', {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
    {'6', {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}},
    {'7', {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
    {'8', {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}},
    {'9', {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},
    {'.', {0x00,0x00,0x00,0x00,0x00,0x04,0x04}},
    {' ', {0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
};
#define FONT_TABLE_SIZE (sizeof(font_table) / sizeof(font_table[0]))

// ─── DISPLAY FUNCTIONS ───────────────────────────────────────────
static void p10_gpio_init(void)
{
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

static const uint8_t* font_find(char c)
{
    for (int i = 0; i < FONT_TABLE_SIZE; i++) {
        if (font_table[i].ch == c) return font_table[i].rows;
    }
    return NULL;
}

static void p10_draw_char(int x_offset, int y_offset, char c)
{
    const uint8_t *glyph = font_find(c);
    if (glyph == NULL) return;
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            uint8_t on = (glyph[row] >> (4 - col)) & 1;
            p10_set_pixel(x_offset + col, y_offset + row, on);
        }
    }
}

static void p10_draw_string(int x_start, int y_start, const char *str)
{
    int x = x_start;
    while (*str) {
        p10_draw_char(x, y_start, *str);
        x += 6;
        str++;
    }
}

static void p10_draw_string_centered(const char *str)
{
    int len = strlen(str);
    int text_width = (len * 6) - 1;
    int x_start = (TOTAL_WIDTH - text_width) / 2;
    int y_start = (TOTAL_HEIGHT - 7) / 2;
    p10_draw_string(x_start, y_start, str);
}

static void p10_send_row(uint8_t scan_row)
{
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
    esp_rom_delay_us(BRIGHTNESS_US);
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

// ─── ESP-NOW RECEIVER ────────────────────────────────────────────
static void on_data_recv(const esp_now_recv_info_t *recv_info,
                         const uint8_t *data, int len)
{
    if (len != sizeof(espnow_msg_t)) {
        ESP_LOGW(TAG, "Paket ukuran tidak dikenal: %d bytes", len);
        return;
    }

    espnow_msg_t *msg = (espnow_msg_t *)data;

    switch (msg->cmd) {
        case CMD_PREPARE:
            ESP_LOGI(TAG, "CMD: PREPARE");
            p10_clear();
            p10_draw_string_centered("PREPARE");
            break;
        case CMD_START:
            ESP_LOGI(TAG, "CMD: START");
            p10_clear();
            p10_draw_string_centered("START");
            break;
        case CMD_FAULT:
            ESP_LOGI(TAG, "CMD: FAULT");
            p10_clear();
            p10_draw_string_centered("FAULT");
            break;
        case CMD_REFUSAL:
            ESP_LOGI(TAG, "CMD: REFUSAL");
            p10_clear();
            p10_draw_string_centered("REFUSAL");
            break;
        case CMD_ELIMINATE:
            ESP_LOGI(TAG, "CMD: ELIMINATE");
            p10_clear();
            p10_draw_string_centered("ELIMINATE");
            break;
        case CMD_PHASE1:
            ESP_LOGI(TAG, "CMD: PHASE1");
            p10_clear();
            p10_draw_string_centered("PHASE1");
            break;
        case CMD_FINISH:
            ESP_LOGI(TAG, "CMD: FINISH");
            p10_clear();
            p10_draw_string_centered("FINISH");
            break;
        case CMD_HEARTBEAT:
            ESP_LOGI(TAG, "CMD: HEARTBEAT");
            break;
        default:
            ESP_LOGW(TAG, "CMD tidak dikenal: 0x%02X", msg->cmd);
            break;
    }
}

static void espnow_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
    esp_wifi_start();

    esp_now_init();
    esp_now_register_recv_cb(on_data_recv);

    ESP_LOGI(TAG, "ESP-NOW receiver ready");
}

// ─── MAIN ────────────────────────────────────────────────────────
void app_main(void)
{
    ESP_LOGI(TAG, "P10 Display starting...");
    p10_gpio_init();
    p10_spi_init();
    p10_clear();
    p10_draw_string_centered("READY");

    espnow_init();

    xTaskCreatePinnedToCore(p10_refresh_task, "p10_refresh", 4096, NULL, 5, NULL, 1);
}
