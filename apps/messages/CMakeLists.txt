set(sources)
set(dependencies uilv lvgl platform appmgr messaging contacts tts audio_board mn316_ctrl)
if(CONFIG_BEPIO_APP_MESSAGES)
    list(APPEND sources "app.c" "model.c" "view.c" "controller.c" "voice.c"
        "pages/messages_page_common.c"
        "pages/messages_list.c"
        "pages/messages_detail.c"
        "pages/messages_compose.c"
        "pages/messages_confirm.c")
    if(CONFIG_BEPIO_MESSAGES_SHELL)
        list(APPEND sources "shell.c")
    endif()
endif()
idf_component_register(SRCS ${sources} INCLUDE_DIRS "../.." "." REQUIRES ${dependencies})
