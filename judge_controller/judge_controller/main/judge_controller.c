#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

#define UART_NUM    UART_NUM_0
#define BUF_SIZE    256
#define MAX_CMD_LEN 64


static void uart_task(void *pvParameter)
{
    char cmd_buf[MAX_CMD_LEN];
    int  cmd_idx = 0;
    uint8_t byte;

    while (1) {
        int len = uart_read_bytes(UART_NUM, &byte, 1, portMAX_DELAY);
        if (len <= 0) continue;

        if (byte == '\r' || byte == '\n') {
            if (cmd_idx > 0) {
                cmd_buf[cmd_idx] = '\0';

                char reply[MAX_CMD_LEN + 8];
                snprintf(reply, sizeof(reply), "ok_%s\r\n", cmd_buf);
                uart_write_bytes(UART_NUM, reply, strlen(reply));

                cmd_idx = 0;
            }
        } else {
            if (cmd_idx < MAX_CMD_LEN - 1) {
                cmd_buf[cmd_idx++] = (char)byte;
            }
        }
    }
}

void app_main(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);

    xTaskCreate(uart_task, "uart_task", 2048, NULL, 5, NULL);
}