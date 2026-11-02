set(sources)
set(dependencies uilv lvgl platform appmgr account)
if(CONFIG_BEPIO_APP_CONTACTS)
    list(APPEND sources "app.c" "model.c" "view.c" "controller.c"
        "pages/contacts_page_common.c"
        "pages/contacts_list.c"
        "pages/contacts_detail.c"
        "pages/contacts_edit.c"
    )
    if(CONFIG_BEPIO_CONTACTS_SHELL)
        list(APPEND sources "shell.c")
    endif()
endif()
idf_component_register(SRCS ${sources} INCLUDE_DIRS "../.." "." REQUIRES ${dependencies})

