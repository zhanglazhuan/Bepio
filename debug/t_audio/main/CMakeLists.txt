option(BEPIO_AUDIO_CAPTURE_ONLY "Read-only I2C capture without codec initialization" OFF)
if(BEPIO_AUDIO_CAPTURE_ONLY)
    idf_component_register(SRCS "t_audio_capture.c" INCLUDE_DIRS "."
                           PRIV_REQUIRES esp32s3 esp_driver_gpio esp_driver_i2c esp_driver_i2s)
else()
    idf_component_register(SRCS "t_audio.c" "audio_signal.c" INCLUDE_DIRS "."
                           PRIV_REQUIRES audio_board esp32s3 console esp_timer)
endif()
