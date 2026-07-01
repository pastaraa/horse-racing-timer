#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "p10_display.h"

static const char *TAG = "RD";

typedef enum {
    STATE_IDLE,
    STATE_PREPARE,
    STATE_START,
    STATE_PHASE,
    STATE_ELIMINATE,
} display_state_t;

static display_state_t current_state    = STATE_IDLE;
static int fault_points                 = 0;
static int refusal_count                = 0;
static int64_t start_time_us            = 0;
static int64_t start_time2_us           = 0;
static int countdown                    = 45;
static int64_t last_countdown_tick      = 0;

void handle_cmd(uint8_t cmd) {
    switch (cmd) {
        case 0x47:
            current_state       = STATE_PREPARE;
            fault_points        = 0;
            refusal_count       = 0;
            countdown           = 45;
            last_countdown_tick = esp_timer_get_time();
            ESP_LOGI(TAG, "→ PREPARE");
            break;
        case 0x51:
            current_state = STATE_START;
            start_time_us = esp_timer_get_time();
            ESP_LOGI(TAG, "→ START");
            break;
        case 0x48:
            fault_points += 4;
            ESP_LOGI(TAG, "→ FAULT, total: %d", fault_points);
            break;
        case 0x49:
            fault_points += 4;
            refusal_count++;
            ESP_LOGI(TAG, "→ REFUSAL, count: %d, total: %d", refusal_count, fault_points);
            if (refusal_count >= 2) {
                current_state = STATE_ELIMINATE;
                ESP_LOGI(TAG, "→ 2x REFUSAL → ELIMINATE");
            }
            break;
        case 0x4A:
            current_state = STATE_ELIMINATE;
            ESP_LOGI(TAG, "→ ELIMINATE");
            break;
        case 0x52:
            current_state  = STATE_PHASE;
            start_time2_us = esp_timer_get_time();
            ESP_LOGI(TAG, "→ PHASE");
            break;
        case 0x53:
            current_state = STATE_IDLE;
            fault_points  = 0;
            refusal_count = 0;
            ESP_LOGI(TAG, "→ FINISH → IDLE");
            break;
        case 0x01:
            current_state = STATE_IDLE;
            ESP_LOGI(TAG, "→ SPLASH → logo");
            break;
        case 0x63:
            ESP_LOGI(TAG, "→ HEARTBEAT");
            break;
        default:
            ESP_LOGW(TAG, "CMD tidak dikenal: 0x%02X", cmd);
            break;
    }
}

static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len < 2) return;
    ESP_LOGI(TAG, "Packet masuk! CMD=0x%02X", data[1]);
    handle_cmd(data[1]);
}

static void espnow_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
    esp_now_init();
    esp_now_register_recv_cb(on_data_recv);
    ESP_LOGI(TAG, "ESP-NOW receiver ready, channel 6");
}

static void serial_input_task(void *pv) {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    ESP_LOGI(TAG, "Ketik: p=PREPARE s=START f=FAULT r=REFUSAL e=ELIMINATE h=PHASE x=FINISH");

    uint8_t ch;
    while (1) {
        if (uart_read_bytes(UART_NUM_0, &ch, 1, portMAX_DELAY) > 0) {
            switch (ch) {
                case 'p': handle_cmd(0x47); break;
                case 's': handle_cmd(0x51); break;
                case 'f': handle_cmd(0x48); break;
                case 'r': handle_cmd(0x49); break;
                case 'e': handle_cmd(0x4A); break;
                case 'h': handle_cmd(0x52); break;
                case 'x': handle_cmd(0x53); break;
                default: break;
            }
        }
    }
}

static void display_task(void *pv) {
    char buf[32];
    while (1) {
        p10_clear();
        switch (current_state) {
            case STATE_IDLE:
                p10_draw_logo();
                break;
            case STATE_PREPARE:
                if (esp_timer_get_time() - last_countdown_tick >= 1000000) {
                    last_countdown_tick = esp_timer_get_time();
                    if (countdown > 0) countdown--;
                }
                snprintf(buf, sizeof(buf), "%d", countdown);
                p10_string_to_buffer(buf, 0, 0, ALIGN_CENTER, ALIGN_MIDDLE);
                break;
            case STATE_START:
            {
                int64_t elapsed = esp_timer_get_time() - start_time_us;
                int tenth   = (int)(elapsed / 100000);
                int detik   = tenth / 10;
                int desimal = tenth % 10;
                snprintf(buf, sizeof(buf), "%d.%d", detik, desimal);
                p10_string_to_buffer(buf, 0, 0, ALIGN_CENTER, ALIGN_MIDDLE);
                if (fault_points > 0) {
                    char fbuf[16];
                    snprintf(fbuf, sizeof(fbuf), "+%d", fault_points);
                    p10_string_to_buffer(fbuf, 0, 0, ALIGN_RIGHT, ALIGN_TOP);
                }
                break;
            }
            case STATE_PHASE:
            {
                int64_t elapsed2 = esp_timer_get_time() - start_time2_us;
                int tenth2   = (int)(elapsed2 / 100000);
                int detik2   = tenth2 / 10;
                int desimal2 = tenth2 % 10;
                snprintf(buf, sizeof(buf), "%d.%d", detik2, desimal2);
                p10_string_to_buffer(buf, 0, 0, ALIGN_CENTER, ALIGN_MIDDLE);
                if (fault_points > 0) {
                    char fbuf[16];
                    snprintf(fbuf, sizeof(fbuf), "+%d", fault_points);
                    p10_string_to_buffer(fbuf, 0, 0, ALIGN_RIGHT, ALIGN_TOP);
                }
                break;
            }
            case STATE_ELIMINATE:
                 p10_draw_eliminate();
                 break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void) { 
    espnow_init();
    p10_init();
    p10_set_brightness(50);
    current_state = STATE_IDLE;

    xTaskCreatePinnedToCore(display_task,      "display", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(serial_input_task, "serial",  4096, NULL, 4, NULL, 0);
}