idf_component_register(SRCS "bepio_mn316_service.c" "onenet.c" "mn316_tls.c" "pager_message.c"
                       INCLUDE_DIRS "."
                       REQUIRES mn316
                       PRIV_REQUIRES esp32s3 esp_driver_uart esp_timer mbedtls json
                       EMBED_TXTFILES "emqx_ca.pem")
