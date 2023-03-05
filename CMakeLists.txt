cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(usb2serial)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

# rest of your project
add_executable(usb2serial
    src/main.cpp
    src/ws2812.cpp
    src/usb_cdc.c
    src/mqtt.cpp
    )

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)
# generate the header file into the source tree as it is included in the RP2040 datasheet
pico_generate_pio_header(usb2serial ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

# pull in common dependencies
target_link_libraries(usb2serial
    pico_stdlib
    hardware_adc
    hardware_pio
    )

# enable usb output, disable uart output
pico_enable_stdio_usb(usb2serial 1)
pico_enable_stdio_uart(usb2serial 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(usb2serial)
