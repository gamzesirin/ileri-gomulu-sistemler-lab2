idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES ortak esp_driver_gpio esp_hw_support soc log freertos)
