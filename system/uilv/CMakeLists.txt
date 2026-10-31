idf_component_register(SRCS "bepio_ui.c" "bepio_ui_preferences.c" "bepio_ui_display.c" "widgets/lv_page.c" "widgets/lv_floating_button.c" "widgets/lv_status_bar.c"
                            "widgets/bepio_navigation.c" "imgs/icon_28x28/ic_send_28.c"
                            "imgs/icon_24x24/ic_visibility_24.c" "imgs/icon_24x24/ic_visibility_off_24.c"
                            "imgs/icon_28x28/ic_bookmark_star_28.c"
                            "imgs/icon_40x40/ic_contacts.c" "imgs/icon_40x40/ic_mail.c"
                            "imgs/icon_40x40/ic_settings.c" "imgs/icon_40x40/ic_voice_chat.c" INCLUDE_DIRS "." "widgets"
                       REQUIRES lvgl PRIV_REQUIRES esp32s3 st7789 ft6336 esp_timer esp_driver_gpio)
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
