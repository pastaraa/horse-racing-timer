#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_timer.h"

#define UART_NUM        UART_NUM_0
#define BUF_SIZE        256
#define MAX_CMD_LEN     64
#define ESPNOW_CHANNEL  6

static const char *TAG = "JCI";
static uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct {
    uint8_t ver;
    uint8_t cmd;
    uint8_t p1Mode;
    uint8_t p2Mode;
    uint8_t p1Time;
    uint8_t p2Time;
    uint32_t timeStamp;
} __attribute__((packed)) espnow_msg_t;

typedef enum {
    CMD_PREPARE  = 0x47,
    CMD_START    = 0x51,
    CMD_FAULT    = 0x48,
    CMD_REFUSAL  = 0x49,
    CMD_ELIMINATE = 0x4A,
    CMD_PHASE1   = 0x52,
    CMD_FINISH   = 0x53,
    CMD_HEARTBEAT = 0x63,
} espnow_cmd_t;

static uint8_t parse_cmd(const char *str) {
    if (strcmp(str, "PREPARE") == 0)  return CMD_PREPARE;
    if (strcmp(str, "START") == 0)    return CMD_START;
    if (strcmp(str, "FAULT") == 0)    return CMD_FAULT;
    if (strcmp(str, "REFUSAL") == 0)  return CMD_REFUSAL;
    if (strcmp(str, "ELIMINATE") == 0) return CMD_ELIMINATE;
    if (strcmp(str, "PHASE1") == 0)   return CMD_PHASE1;
    if (strcmp(str, "FINISH") == 0)   return CMD_FINISH;
    return 0x00;
}

static void send_espnow_cmd(uint8_t cmd) {
    espnow_msg_t msg = {
        .ver       = 0x01,
        .cmd       = cmd,
        .p1Mode    = 0x00,
        .p2Mode    = 0x00,
        .p1Time    = 0x00,
        .p2Time    = 0x00,
        .timeStamp = (uint32_t)(esp_timer_get_time() / 1000),
    };
    esp_now_send(broadcast_mac, (uint8_t *)&msg, sizeof(msg));
    ESP_LOGI(TAG, "ESP-NOW sent: cmd=0x%02X", cmd);
}

static void uart_task(void *pvParameter) {
    char cmd_buf[MAX_CMD_LEN];
    int  cmd_idx = 0;
    uint8_t byte;

    while (1) {
        int len = uart_read_bytes(UART_NUM, &byte, 1, portMAX_DELAY);
        if (len <= 0) continue;

        if (byte == '\r' || byte == '\n') {
            if (cmd_idx > 0) {
                cmd_buf[cmd_idx] = '\0';

                uint8_t cmd = parse_cmd(cmd_buf);
                if (cmd != 0x00) {
                    send_espnow_cmd(cmd);
                    char reply[MAX_CMD_LEN + 8];
                    snprintf(reply, sizeof(reply), "ok_%s\r\n", cmd_buf);
                    uart_write_bytes(UART_NUM, reply, strlen(reply));
                } else {
                    uart_write_bytes(UART_NUM, "err_unknown\r\n", 13);
                    ESP_LOGW(TAG, "Unknown command: %s", cmd_buf);
                }

                cmd_idx = 0;
            }
        } else {
            if (cmd_idx < MAX_CMD_LEN - 1) {
                cmd_buf[cmd_idx++] = (char)byte;
            }
        }
    }
}

static void on_data_sent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "ESP-NOW send OK");
    } else {
        ESP_LOGW(TAG, "ESP-NOW send FAIL");
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
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

    esp_now_init();
    esp_now_register_send_cb(on_data_sent);

    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, broadcast_mac, 6);
    peer_info.channel = ESPNOW_CHANNEL;
    peer_info.encrypt = false;
    esp_now_add_peer(&peer_info);

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);

    xTaskCreate(uart_task, "uart_task", 4096, NULL, 5, NULL);
}