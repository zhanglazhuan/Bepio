idf_component_register(SRCS "bepio_lcd.c" INCLUDE_DIRS "."
                       REQUIRES esp32s3 PRIV_REQUIRES esp_timer esp_lcd esp_driver_spi esp_driver_gpio)
