idf_component_register(SRCS "main.c" "p10_display.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_wifi esp_netif esp_driver_gpio esp_driver_spi esp_driver_uart esp_timer)