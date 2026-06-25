#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "p10_display.h"

static const char *TAG = "RD";

typedef enum {
    STATE_IDLE,
    STATE_PREPARE,
    STATE_START,
    STATE_ELIMINATE,
    STATE_FINISH
} display_state_t;

static display_state_t current_state = STATE_IDLE;
static int fault_points = 0;
static int64_t start_time_us = 0;
static int countdown = 45;
static int64_t last_countdown_tick = 0;

void handle_cmd(uint8_t cmd) {
    switch (cmd) {
        case 0x47:
            current_state = STATE_PREPARE;
            fault_points = 0;
            countdown = 45;
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
        case 0x4A:
            current_state = STATE_ELIMINATE;
            ESP_LOGI(TAG, "→ ELIMINATE");
            break;
        case 0x53:
            current_state = STATE_FINISH;
            ESP_LOGI(TAG, "→ FINISH");
            break;
        default:
            break;
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
                int tenth = (int)(elapsed / 100000);
                int detik = tenth / 10;
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
            case STATE_ELIMINATE:
                p10_string_to_buffer("ELIMINATE", 0, 0, ALIGN_CENTER, ALIGN_MIDDLE);
                break;
            case STATE_FINISH:
                p10_clear();
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void) {
    p10_init();
    p10_set_brightness(100);
    current_state = STATE_IDLE;

    xTaskCreatePinnedToCore(display_task, "display", 4096, NULL, 5, NULL, 0);

    // Simulasi CMD
    vTaskDelay(pdMS_TO_TICKS(2000));
    handle_cmd(0x47); // PREPARE
    vTaskDelay(pdMS_TO_TICKS(5000));
    handle_cmd(0x51); // START
    vTaskDelay(pdMS_TO_TICKS(3000));
    handle_cmd(0x48); // FAULT
    vTaskDelay(pdMS_TO_TICKS(3000));
    handle_cmd(0x48); // FAULT lagi
    vTaskDelay(pdMS_TO_TICKS(3000));
    handle_cmd(0x53); // FINISH

    while(1) vTaskDelay(pdMS_TO_TICKS(1000));
}