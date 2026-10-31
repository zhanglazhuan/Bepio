cmake_minimum_required(VERSION 3.16)
project(launcher_preview C CXX ASM)
get_filename_component(BEPIO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${BEPIO_ROOT}/managed_components/lvgl__lvgl" lvgl)
add_executable(launcher_preview main.c "${BEPIO_ROOT}/system/launcher/bepio_launcher.c")
target_sources(launcher_preview PRIVATE "${BEPIO_ROOT}/system/uilv/imgs/icon_40x40/ic_contacts.c")
target_sources(launcher_preview PRIVATE
    "${BEPIO_ROOT}/system/uilv/imgs/icon_40x40/ic_mail.c"
    "${BEPIO_ROOT}/system/uilv/imgs/icon_40x40/ic_settings.c")
target_include_directories(launcher_preview PRIVATE host "${BEPIO_ROOT}" "${BEPIO_ROOT}/system/launcher" "${BEPIO_ROOT}/system/appmgr")
target_link_libraries(launcher_preview PRIVATE lvgl m)

add_executable(contacts_preview contacts_main.c
    "${BEPIO_ROOT}/system/uilv/imgs/icon_28x28/ic_send_28.c"
    "${BEPIO_ROOT}/system/uilv/imgs/icon_28x28/ic_bookmark_star_28.c"
    "${BEPIO_ROOT}/apps/contacts/app.c" "${BEPIO_ROOT}/apps/contacts/model.c"
    "${BEPIO_ROOT}/apps/contacts/controller.c" "${BEPIO_ROOT}/apps/contacts/view.c"
    "${BEPIO_ROOT}/apps/contacts/pages/contacts_page_common.c"
    "${BEPIO_ROOT}/apps/contacts/pages/contacts_list.c"
    "${BEPIO_ROOT}/apps/contacts/pages/contacts_detail.c"
    "${BEPIO_ROOT}/apps/contacts/pages/contacts_edit.c"
    "${BEPIO_ROOT}/apps/contacts/pages/contacts_delete_confirm.c"
    "${BEPIO_ROOT}/apps/contacts/pages/contacts_discard_confirm.c"
 "${BEPIO_ROOT}/system/appmgr/bepio_app_manager.c")
target_include_directories(contacts_preview PRIVATE
    "${BEPIO_ROOT}/tests/unittest/app_manager/host" "${BEPIO_ROOT}"
    "${BEPIO_ROOT}/apps/contacts" "${BEPIO_ROOT}/system/appmgr")
target_compile_definitions(contacts_preview PRIVATE CONFIG_BEPIO_MAX_APPS=8
    CONFIG_BEPIO_CONTACTS_CAPACITY=32)
target_link_libraries(contacts_preview PRIVATE lvgl m)

set(BEPIO_WIDGET_SOURCES
    "${BEPIO_ROOT}/system/uilv/widgets/lv_page.c"
    "${BEPIO_ROOT}/system/uilv/widgets/lv_status_bar.c"
    "${BEPIO_ROOT}/system/uilv/widgets/bepio_navigation.c")

foreach(target launcher_preview contacts_preview)
    target_sources(${target} PRIVATE ${BEPIO_WIDGET_SOURCES})
    target_include_directories(${target} PRIVATE "${BEPIO_ROOT}/system/uilv/widgets")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endforeach()
target_compile_definitions(launcher_preview PRIVATE CONFIG_BEPIO_CONTACTS_CAPACITY=32)
add_executable(display_pipeline_preview display_pipeline_main.c
    "${BEPIO_ROOT}/system/uilv/bepio_ui_display.c"
    "${BEPIO_ROOT}/drivers/st7789/bepio_lcd.c"
    "${BEPIO_ROOT}/tests/unittest/display_async/sdk_mock.c")
target_include_directories(display_pipeline_preview PRIVATE
    "${BEPIO_ROOT}/tests/unittest/display_async/host"
    "${BEPIO_ROOT}/tests/unittest/display_async"
    "${BEPIO_ROOT}/system/uilv" "${BEPIO_ROOT}/drivers/st7789" "${BEPIO_ROOT}/boards/esp32s3")
target_compile_options(display_pipeline_preview PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(display_pipeline_preview PRIVATE lvgl m)
enable_testing()
foreach(scenario success timeout submit-error oom wait-timeout)
    add_test(NAME display_pipeline_${scenario} COMMAND display_pipeline_preview ${scenario})
endforeach()

add_executable(settings_preview settings_main.c
    "${BEPIO_ROOT}/system/uilv/imgs/icon_24x24/ic_visibility_24.c"
    "${BEPIO_ROOT}/system/uilv/imgs/icon_24x24/ic_visibility_off_24.c"
    "${BEPIO_ROOT}/apps/settings/app.c" "${BEPIO_ROOT}/apps/settings/model.c"
    "${BEPIO_ROOT}/apps/settings/controller.c" "${BEPIO_ROOT}/apps/settings/view.c"
    "${BEPIO_ROOT}/apps/settings/pages/settings_page_common.c"
    "${BEPIO_ROOT}/apps/settings/pages/settings_home.c"
    "${BEPIO_ROOT}/apps/settings/pages/settings_id.c"
    "${BEPIO_ROOT}/apps/settings/pages/settings_datetime.c"
    "${BEPIO_ROOT}/apps/settings/pages/settings_battery.c"
    "${BEPIO_ROOT}/apps/settings/pages/settings_updates.c"
    "${BEPIO_ROOT}/apps/settings/pages/settings_about.c"
    "${BEPIO_ROOT}/apps/settings/pages/settings_wifi.c"
    "${BEPIO_ROOT}/apps/settings/pages/settings_wifi_connect.c"
    "${BEPIO_ROOT}/apps/settings/pages/settings_usage.c"
    "${BEPIO_ROOT}/apps/settings/pages/settings_storage.c"
    "${BEPIO_ROOT}/system/appmgr/bepio_app_manager.c" ${BEPIO_WIDGET_SOURCES})
target_include_directories(settings_preview PRIVATE
    "${BEPIO_ROOT}/tests/unittest/app_manager/host" "${BEPIO_ROOT}"
    "${BEPIO_ROOT}/system/uilv" "${BEPIO_ROOT}/system/uilv/widgets"
    "${BEPIO_ROOT}/system/platform" "${BEPIO_ROOT}/system/services/account" "${BEPIO_ROOT}/system/services/wifi"
    "${BEPIO_ROOT}/system/appmgr" "${BEPIO_ROOT}/system/services/messaging")
target_compile_definitions(settings_preview PRIVATE CONFIG_BEPIO_MAX_APPS=8 CONFIG_BEPIO_APP_MESSAGES=1)
target_compile_options(settings_preview PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(settings_preview PRIVATE lvgl m)

add_executable(preferences_preview preferences_main.c "${BEPIO_ROOT}/system/uilv/bepio_ui_preferences.c")
target_include_directories(preferences_preview PRIVATE preferences_host "${BEPIO_ROOT}/system/uilv" "${BEPIO_ROOT}/system/uilv/widgets")
target_compile_definitions(preferences_preview PRIVATE setenv=bepio_test_setenv localtime_r=bepio_test_localtime_r)
target_compile_options(preferences_preview PRIVATE -Wall -Wextra -Werror -UNDEBUG -include "${CMAKE_CURRENT_LIST_DIR}/preferences_host/compat.h")
target_link_libraries(preferences_preview PRIVATE lvgl m)

add_executable(floating_button_preview floating_button_main.c
    "${BEPIO_ROOT}/system/uilv/widgets/lv_floating_button.c"
    "${BEPIO_ROOT}/system/uilv/imgs/icon_28x28/ic_send_28.c" ${BEPIO_WIDGET_SOURCES})
target_include_directories(floating_button_preview PRIVATE "${BEPIO_ROOT}" "${BEPIO_ROOT}/system/uilv/widgets")
target_compile_options(floating_button_preview PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(floating_button_preview PRIVATE lvgl m)

add_test(NAME floating_button COMMAND floating_button_preview "${CMAKE_CURRENT_BINARY_DIR}")
