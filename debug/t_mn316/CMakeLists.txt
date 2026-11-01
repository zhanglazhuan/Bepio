cmake_minimum_required(VERSION 3.16)
set(IDF_TARGET "esp32s3" CACHE STRING "Test MCU")
set(EXTRA_COMPONENT_DIRS "../../boards/esp32s3" "../../drivers/mn316"
                        "../../system/mn316_ctrl")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
idf_build_set_property(BEPIO_MN316_STANDALONE ON)
project(t_mn316)
