#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#define LED_PIN GPIO_NUM_2
#define SEND_INTERVAL_MS 3000

static const char *TAG = "ESPNOW_SENDER";
static uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Jenis event
typedef enum {
    EVENT_START  = 1,
    EVENT_FINISH = 2,
} race_event_t;

// Paket data yang dikirim
typedef struct {
    race_event_t event;
    uint8_t sensor_id;
    uint8_t minutes;
    uint8_t seconds;
    uint16_t milliseconds;
} race_data_t;

// Fungsi konversi waktu dari millisecond total
static void ms_to_time(uint32_t total_ms, uint8_t *min, uint8_t *sec, uint16_t *ms) {
    *ms  = total_ms % 1000;
    *sec = (total_ms / 1000) % 60;
    *min = (total_ms / 60000);
}

static void on_data_sent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "Send OK");
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(LED_PIN, 0);
    } else {
        ESP_LOGW(TAG, "Send FAIL");
    }
}

static void espnow_send_task(void *pvParameter) {
    race_data_t data = {0};
    bool is_start = true; // toggle START/FINISH

    while (1) {
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);

        data.event     = is_start ? EVENT_START : EVENT_FINISH;
        data.sensor_id = is_start ? 1 : 2;
        ms_to_time(now_ms, &data.minutes, &data.seconds, &data.milliseconds);

        esp_now_send(broadcast_mac, (uint8_t *)&data, sizeof(data));

        ESP_LOGI(TAG, "Sending event: %s | sensor: %d | time: %02d:%02d:%03d",
                 data.event == EVENT_START ? "START" : "FINISH",
                 data.sensor_id,
                 data.minutes, data.seconds, data.milliseconds);

        is_start = !is_start; // toggle
        vTaskDelay(pdMS_TO_TICKS(SEND_INTERVAL_MS));
    }
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_now_init();
    esp_now_register_send_cb(on_data_sent);

    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, broadcast_mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;
    esp_now_add_peer(&peer_info);

    xTaskCreate(espnow_send_task, "espnow_send", 4096, NULL, 5, NULL);
}