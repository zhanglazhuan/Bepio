set(sources)
if(CONFIG_BEPIO_APP_CHAT)
    list(APPEND sources "app.c" "model.c" "controller.c" "view.c")
endif()
idf_component_register(SRCS ${sources} INCLUDE_DIRS "../.." "."
    REQUIRES appmgr uilv lvgl contacts messages messaging)
