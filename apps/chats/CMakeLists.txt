set(sources)
if(CONFIG_BEPIO_APP_CHATS)
    list(APPEND sources "app.c" "model.c" "controller.c" "view.c" "pages/chats_main.c")
endif()
idf_component_register(SRCS ${sources} INCLUDE_DIRS "../.." "."
    REQUIRES appmgr uilv lvgl contacts messages messaging)
