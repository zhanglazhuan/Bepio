cmake_minimum_required(VERSION 3.16)
project(messages_preview C CXX ASM)
get_filename_component(BEPIO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(LV_BUILD_CONF_PATH "${BEPIO_ROOT}/tests/integration/launcher_preview/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${BEPIO_ROOT}/managed_components/lvgl__lvgl" lvgl)
add_executable(messages_preview main.c
	"${BEPIO_ROOT}/system/uilv/imgs/icon_28x28/ic_send_28.c"
    "${BEPIO_ROOT}/apps/messages/view.c"
    "${BEPIO_ROOT}/apps/messages/pages/messages_page_common.c"
    "${BEPIO_ROOT}/apps/messages/pages/messages_list.c"
    "${BEPIO_ROOT}/apps/messages/pages/messages_detail.c"
    "${BEPIO_ROOT}/apps/messages/pages/messages_compose.c"
    "${BEPIO_ROOT}/apps/messages/pages/messages_confirm.c"
    "${BEPIO_ROOT}/system/uilv/framework/page_navigator.c"
    "${BEPIO_ROOT}/apps/messages/model.c"
    "${BEPIO_ROOT}/apps/messages/controller.c"
    "${BEPIO_ROOT}/system/services/messaging/messaging.c")
target_include_directories(messages_preview PRIVATE "${BEPIO_ROOT}/system/uilv/framework"
    "${BEPIO_ROOT}/tests/integration/launcher_preview/host"
    "${BEPIO_ROOT}/tests/unittest/app_manager/host"
    "${BEPIO_ROOT}/apps/messages" "${BEPIO_ROOT}/system/services/messaging")
target_compile_definitions(messages_preview PRIVATE BEPIO_MESSAGES_PREVIEW=1 CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY=20 CONFIG_BEPIO_MESSAGES_OUTBOX_CAPACITY=20)
target_compile_options(messages_preview PRIVATE -Wall -Wextra -Werror -UNDEBUG -include "${BEPIO_ROOT}/tests/unittest/messages/host/errno_compat.h")
target_link_libraries(messages_preview PRIVATE lvgl m)

if(WIN32)
    target_link_libraries(messages_preview PRIVATE user32 gdi32)
endif()

set(BEPIO_WIDGET_SOURCES
    "${BEPIO_ROOT}/system/uilv/widgets/lv_page.c"
    "${BEPIO_ROOT}/system/uilv/widgets/lv_status_bar.c"
    )

target_sources(messages_preview PRIVATE ${BEPIO_WIDGET_SOURCES})
target_include_directories(messages_preview PRIVATE "${BEPIO_ROOT}/system/uilv/widgets")

target_sources(messages_preview PRIVATE
    "${BEPIO_ROOT}/apps/contacts/app.c" "${BEPIO_ROOT}/apps/contacts/model.c"
    "${BEPIO_ROOT}/apps/contacts/controller.c"
    "${BEPIO_ROOT}/tests/unittest/contacts/view_mock.c"
    "${BEPIO_ROOT}/system/appmgr/bepio_app_manager.c")
target_compile_definitions(messages_preview PRIVATE CONFIG_BEPIO_APP_CONTACTS=1
    CONFIG_BEPIO_CONTACTS_CAPACITY=32 CONFIG_BEPIO_MAX_APPS=8)
target_include_directories(messages_preview PRIVATE "${BEPIO_ROOT}" "${BEPIO_ROOT}/system/appmgr")
