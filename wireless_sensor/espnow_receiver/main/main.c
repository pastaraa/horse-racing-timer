#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define LED_PIN GPIO_NUM_2

static const char *TAG = "ESPNOW_RECEIVER";

// Jenis event (harus sama persis dengan sender!)
typedef enum {
    EVENT_START  = 1,
    EVENT_FINISH = 2,
} race_event_t;

// Paket data yang diterima (harus sama persis dengan sender!)
typedef struct {
    race_event_t event;
    uint8_t sensor_id;
    uint8_t minutes;
    uint8_t seconds;
    uint16_t milliseconds;
} race_data_t;

static void blink_led(void) {
    gpio_set_level(LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LED_PIN, 0);
}

static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(race_data_t)) {
        race_data_t *recv_data = (race_data_t *)data;

        ESP_LOGI(TAG, "Received event: %s | sensor: %d | time: %02d:%02d:%03d",
                 recv_data->event == EVENT_START ? "START" : "FINISH",
                 recv_data->sensor_id,
                 recv_data->minutes,
                 recv_data->seconds,
                 recv_data->milliseconds);
    } else {
        ESP_LOGW(TAG, "Unknown data, len=%d", len);
    }
    blink_led();
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
    esp_now_register_recv_cb(on_data_recv);

    ESP_LOGI(TAG, "Receiver ready, waiting for race events...");
}