cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(usb2serial)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
add_executable(usb2serial
    main.cpp
    )

# pull in common dependencies
target_link_libraries(usb2serial
    pico_stdlib
    )

# enable usb output, disable uart output
pico_enable_stdio_usb(usb2serial 1)
pico_enable_stdio_uart(usb2serial 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(usb2serial)