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

typedef struct {
    uint32_t counter;
    char message[32];
} espnow_data_t;

static void blink_led(void) {
    gpio_set_level(LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LED_PIN, 0);
}

static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(espnow_data_t)) {
        espnow_data_t *recv_data = (espnow_data_t *)data;
        ESP_LOGI(TAG, "Received - counter: %lu, message: %s",
                 recv_data->counter, recv_data->message);
    } else {
        ESP_LOGW(TAG, "Received unknown data, len=%d", len);
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

    ESP_LOGI(TAG, "Receiver ready, waiting for data...");
}