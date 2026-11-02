set(sources "bepio_mn316_manager.c" "bepio_mn316_session.c" "mn316_mqtt.c"
            "pager_message.c" "bepio_mn316_legacy.c" "onenet.c")
set(private_dependencies mbedtls json esp_timer esp_hw_support)
# Requirements are expanded before sdkconfig; use an IDF build property rather
# than CONFIG_* or ordinary project variables to keep both passes consistent.
idf_build_get_property(mn316_standalone BEPIO_MN316_STANDALONE)
if(NOT mn316_standalone)
    list(APPEND private_dependencies messaging account notifications contacts)
endif()
idf_component_register(SRCS ${sources} INCLUDE_DIRS "." REQUIRES mn316
                       PRIV_REQUIRES ${private_dependencies}
                       EMBED_TXTFILES "certs/emqx_ca.pem")
