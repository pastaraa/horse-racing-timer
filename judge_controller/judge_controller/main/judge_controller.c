#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"

static const char *TAG = "JUDGE_CTRL";

// ─── ESP-NOW ─────────────────────────────────────────────────────
static uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ─── Race Data ───────────────────────────────────────────────────
typedef enum {
    EVENT_START  = 1,
    EVENT_FINISH = 2,
} race_event_t;

typedef struct {
    race_event_t event;
    uint8_t      sensor_id;
    uint8_t      minutes;
    uint8_t      seconds;
    uint16_t     milliseconds;
} race_data_t;

// ─── Forward data ke Remote Display via ESP-NOW ──────────────────
static void forward_to_display(race_data_t *data)
{
    esp_now_send(broadcast_mac, (uint8_t *)data, sizeof(race_data_t));
    ESP_LOGI(TAG, "Forwarded to display: %s | %02d:%02d:%03d",
             data->event == EVENT_START ? "START" : "FINISH",
             data->minutes, data->seconds, data->milliseconds);
}

// ─── TODO: Kirim ke laptop Om Yudi via WiFi ──────────────────────
// Akan diimplementasi setelah brief dengan Om Yudi
static void send_to_laptop(race_data_t *data)
{
    // placeholder
    ESP_LOGI(TAG, "TODO: send to laptop app");
}

// ─── ESP-NOW Receive Callback ────────────────────────────────────
static void on_data_recv(const esp_now_recv_info_t *recv_info,
                         const uint8_t *data, int len)
{
    if (len != sizeof(race_data_t)) {
        ESP_LOGW(TAG, "Data size mismatch: %d", len);
        return;
    }

    race_data_t recv_data;
    memcpy(&recv_data, data, sizeof(race_data_t));

    ESP_LOGI(TAG, "Received %s from sensor %d | %02d:%02d:%03d",
             recv_data.event == EVENT_START ? "START" : "FINISH",
             recv_data.sensor_id,
             recv_data.minutes, recv_data.seconds, recv_data.milliseconds);

    forward_to_display(&recv_data);
    send_to_laptop(&recv_data);
}

// ─── App Main ────────────────────────────────────────────────────
void app_main(void)
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
    esp_wifi_start();

    esp_now_init();
    esp_now_register_recv_cb(on_data_recv);

    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, broadcast_mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;
    esp_now_add_peer(&peer_info);

    ESP_LOGI(TAG, "Judge Controller ready, waiting for sensor data...");
}