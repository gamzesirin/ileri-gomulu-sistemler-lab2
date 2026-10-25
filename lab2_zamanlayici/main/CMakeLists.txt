idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES ortak esp_driver_gptimer esp_timer esp_hw_support log freertos
                                esp_wifi esp_netif esp_event nvs_flash)
