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

static const char *TAG = "SENSOR";

// ─── Pin Definitions ─────────────────────────────────────────────
#define LED_PIN           GPIO_NUM_2
#define SENSOR_START_PIN  GPIO_NUM_25
#define SENSOR_FINISH_PIN GPIO_NUM_26

// ─── Debounce ────────────────────────────────────────────────────
#define DEBOUNCE_MS  50

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

// ─── State ───────────────────────────────────────────────────────
static bool race_started  = false;
static bool race_finished = false;
static uint32_t start_time_ms = 0;

// ─── Helper ──────────────────────────────────────────────────────
static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void ms_to_time(uint32_t total_ms, uint8_t *min, uint8_t *sec, uint16_t *ms)
{
    *ms  = total_ms % 1000;
    *sec = (total_ms / 1000) % 60;
    *min = (total_ms / 60000);
}

// ─── Kirim data race ─────────────────────────────────────────────
static void send_race_event(race_event_t event, uint8_t sensor_id, uint32_t elapsed_ms)
{
    race_data_t data = {0};
    data.event     = event;
    data.sensor_id = sensor_id;
    ms_to_time(elapsed_ms, &data.minutes, &data.seconds, &data.milliseconds);

    esp_now_send(broadcast_mac, (uint8_t *)&data, sizeof(data));

    ESP_LOGI(TAG, "Sent %s | sensor: %d | time: %02d:%02d:%03d",
             event == EVENT_START ? "START" : "FINISH",
             sensor_id,
             data.minutes, data.seconds, data.milliseconds);

    // Kedip LED tanda kirim
    gpio_set_level(LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LED_PIN, 0);
}

// ─── Sensor Task ─────────────────────────────────────────────────
static void sensor_task(void *pvParameter)
{
    bool last_start  = true;   // HIGH = tidak ada kuda
    bool last_finish = true;

    while (1) {
        bool cur_start  = gpio_get_level(SENSOR_START_PIN);
        bool cur_finish = gpio_get_level(SENSOR_FINISH_PIN);

        // Sensor START terpicu (HIGH → LOW)
        if (last_start && !cur_start && !race_started) {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));  // debounce
            if (!gpio_get_level(SENSOR_START_PIN)) { // konfirmasi masih LOW
                race_started  = true;
                race_finished = false;
                start_time_ms = now_ms();
                send_race_event(EVENT_START, 1, 0);
            }
        }

        // Sensor FINISH terpicu (HIGH → LOW)
        if (last_finish && !cur_finish && race_started && !race_finished) {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));  // debounce
            if (!gpio_get_level(SENSOR_FINISH_PIN)) { // konfirmasi masih LOW
                race_finished = true;
                uint32_t elapsed = now_ms() - start_time_ms;
                send_race_event(EVENT_FINISH, 2, elapsed);

                // Reset untuk race berikutnya
                race_started  = false;
                race_finished = false;
            }
        }

        last_start  = cur_start;
        last_finish = cur_finish;

        vTaskDelay(pdMS_TO_TICKS(10));  // polling tiap 10ms
    }
}

// ─── App Main ────────────────────────────────────────────────────
void app_main(void)
{
    // NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // LED
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    // Sensor pins — INPUT_PULLUP, active LOW
    gpio_reset_pin(SENSOR_START_PIN);
    gpio_set_direction(SENSOR_START_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(SENSOR_START_PIN, GPIO_PULLUP_ONLY);

    gpio_reset_pin(SENSOR_FINISH_PIN);
    gpio_set_direction(SENSOR_FINISH_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(SENSOR_FINISH_PIN, GPIO_PULLUP_ONLY);

    // WiFi + ESP-NOW
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_now_init();

    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, broadcast_mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;
    esp_now_add_peer(&peer_info);

    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Sensor ready, waiting for race...");
}