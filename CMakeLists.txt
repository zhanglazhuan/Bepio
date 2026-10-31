cmake_minimum_required(VERSION 3.16)
set(IDF_TARGET "esp32s3" CACHE STRING "Bepio MCU")
set(EXTRA_COMPONENT_DIRS
    drivers/st7789
    drivers/ft6336
    system/uilv
    system/launcher
    boards/esp32s3
    drivers/mn316
    drivers/audio_board
    system/platform
    system/controller
    system/appmgr
    system/services/wifi
    system/services/mn316_service
    system/services/account
    system/services/messaging
    system/services/tts
    apps/settings
    apps/contacts
    apps/messages
    apps/chat
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(Bepio)

# Keep graphics hot paths out of the project's default -Og debug build.
# Set this after project(), when IDF component targets exist; do not edit
# managed_components or change optimization for unrelated system services.
if(CONFIG_BEPIO_UI_OPTIMIZE)
    foreach(graphics_component IN ITEMS lvgl__lvgl uilv st7789 esp_lcd)
        idf_component_get_property(graphics_library ${graphics_component} COMPONENT_LIB)
        target_compile_options(${graphics_library} PRIVATE -O2)
    endforeach()
endif()
