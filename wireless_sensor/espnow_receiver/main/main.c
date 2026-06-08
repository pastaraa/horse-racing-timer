#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"

static const char *TAG = "SNIFFER";

typedef struct {
    uint8_t ver;
    uint8_t cmd;
    uint8_t p1Mode;
    uint8_t p2Mode;
    uint8_t p1Time;
    uint8_t p2Time;
    uint32_t timeStamp;
} espnow_msg_t;

static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    ESP_LOGI(TAG, "=== PACKET RECEIVED ===");
    ESP_LOGI(TAG, "From MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
             recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
    ESP_LOGI(TAG, "Raw hex:");
    ESP_LOG_BUFFER_HEX(TAG, data, len);

    if (len >= sizeof(espnow_msg_t)) {
        espnow_msg_t *m = (espnow_msg_t *)data;
        ESP_LOGI(TAG, "ver=%d cmd=%d p1Mode=%d p2Mode=%d p1Time=%d p2Time=%d ts=%lu",
                 m->ver, m->cmd, m->p1Mode, m->p2Mode, m->p1Time, m->p2Time, m->timeStamp);
    } else {
        ESP_LOGW(TAG, "Packet terlalu kecil: %d bytes", len);
    }
}

void app_main(void) {
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

    ESP_LOGI(TAG, "Sniffer ready on channel 6...");
}