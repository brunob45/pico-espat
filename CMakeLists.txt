cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(pico-efi)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

# rest of your project
add_executable(pico-efi
    src/main.cpp
    src/usb_cdc.c
    src/i2c_dma.c
    src/seesaw.cpp
    )

# pull in common dependencies
target_link_libraries(pico-efi
    pico_stdlib
    hardware_i2c
    hardware_dma
    )

# enable usb output, disable uart output
pico_enable_stdio_usb(pico-efi 1)
pico_enable_stdio_uart(pico-efi 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(pico-efi)

install(CODE "execute_process(COMMAND .vscode/upload.ps1)")