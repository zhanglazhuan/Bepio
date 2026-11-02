set(sources)
if(CONFIG_BEPIO_APP_NOTIFICATIONS)
    list(APPEND sources "app.c" "model.c" "controller.c" "view.c")
endif()
idf_component_register(SRCS ${sources} INCLUDE_DIRS "../.." "."
    REQUIRES appmgr uilv lvgl platform)
