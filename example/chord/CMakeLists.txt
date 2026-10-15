cmake_minimum_required(VERSION 3.12)

set(PICO_BOARD $ENV{PICO_BOARD})
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(chord)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_subdirectory(../.. build)

add_executable(chord main.c)

target_link_libraries(chord pico_stdlib pico-button-events)
pico_add_extra_outputs(chord)

pico_enable_stdio_uart(chord 1)
pico_enable_stdio_usb(chord 1)
