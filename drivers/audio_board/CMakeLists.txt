option(BEPIO_AUDIO_HARDWARE_I2C "Use native I2C for codec comparison" OFF)
idf_component_register(SRCS "audio_board.c" "audio_i2c_ctrl.c" "audio_soft_i2c.c" INCLUDE_DIRS "."
                       PRIV_REQUIRES esp32s3 esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_codec_dev esp_timer)
if(NOT BEPIO_AUDIO_HARDWARE_I2C)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE BEPIO_AUDIO_SOFT_I2C=1)
endif()

set(BEPIO_AUDIO_SAMPLE_BITS 16 CACHE STRING "I2S and codec sample width")
if(NOT BEPIO_AUDIO_SAMPLE_BITS MATCHES "^(16|32)$")
    message(FATAL_ERROR "BEPIO_AUDIO_SAMPLE_BITS must be 16 or 32")
endif()
target_compile_definitions(${COMPONENT_LIB} PRIVATE BEPIO_AUDIO_SAMPLE_BITS=${BEPIO_AUDIO_SAMPLE_BITS})
