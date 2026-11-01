set(sources)
set(dependencies uilv lvgl platform appmgr account wifi messaging)
if(CONFIG_BEPIO_APP_SETTINGS)
    list(APPEND sources "app.c" "model.c" "view.c" "controller.c"
        "pages/settings_page_context.c"
        "pages/settings_home.c"
        "pages/settings_id.c"
        "pages/settings_datetime.c"
        "pages/settings_battery.c"
        "pages/settings_updates.c"
        "pages/settings_about.c"
        "pages/settings_wifi.c"
        "pages/settings_wifi_connect.c"
        "pages/settings_usage.c"
        "pages/settings_storage.c"
    )
    if(CONFIG_BEPIO_SETTINGS_SHELL)
        list(APPEND sources "shell.c")
    endif()
endif()
idf_component_register(SRCS ${sources} INCLUDE_DIRS "../.." "." REQUIRES ${dependencies})
