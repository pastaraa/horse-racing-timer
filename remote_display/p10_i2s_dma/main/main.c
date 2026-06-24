#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "p10_display.h"

void app_main(void)
{
    p10_init();

    p10_clear();

    p10_draw_logo();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}