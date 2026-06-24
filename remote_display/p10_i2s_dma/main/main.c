#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "p10_display.h"

void app_main(void)
{
    p10_init();

    int counter = 0; 
    char time_str[20]; 

    while (1) {
        p10_clear();

        int detik = counter / 10;      
        int desimal = counter % 10;     

        snprintf(time_str, sizeof(time_str), "%d.%d", detik, desimal);

        p10_string_to_buffer(time_str, 0, 0, ALIGN_CENTER, ALIGN_MIDDLE);

        counter++; 

        if (counter > 600) counter = 0; 

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}