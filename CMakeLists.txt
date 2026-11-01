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
    system/mn316_ctrl
    system/appmgr
    system/services/wifi
    system/services/account
    system/services/messaging
    system/services/tts
    apps/settings
    apps/contacts
    apps/messages
    apps/chats
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(Bepio)

if(CONFIG_BEPIO_PRODUCTION_BUILD)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    if(DEFINED SDKCONFIG)
        set(release_config "${SDKCONFIG}")
    else()
        set(release_config "${CMAKE_SOURCE_DIR}/sdkconfig")
    endif()
    if(IS_ABSOLUTE "${CONFIG_PARTITION_TABLE_CUSTOM_FILENAME}")
        set(release_partitions "${CONFIG_PARTITION_TABLE_CUSTOM_FILENAME}")
    else()
        set(release_partitions "${CMAKE_SOURCE_DIR}/${CONFIG_PARTITION_TABLE_CUSTOM_FILENAME}")
    endif()
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/check_production.py"
                --config "${release_config}" --partitions "${release_partitions}"
        RESULT_VARIABLE release_result
        OUTPUT_VARIABLE release_output
        ERROR_VARIABLE release_error)
    if(NOT release_result EQUAL 0)
        message(FATAL_ERROR "Production release gate:\n${release_output}${release_error}")
    endif()
endif()

# Keep graphics hot paths out of the project's default -Og debug build.
# Set this after project(), when IDF component targets exist; do not edit
# managed_components or change optimization for unrelated system services.
if(CONFIG_BEPIO_UI_OPTIMIZE)
    foreach(graphics_component IN ITEMS lvgl__lvgl uilv st7789 esp_lcd)
        idf_component_get_property(graphics_library ${graphics_component} COMPONENT_LIB)
        target_compile_options(${graphics_library} PRIVATE -O2)
    endforeach()
endif()
