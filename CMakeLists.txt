cmake_minimum_required(VERSION 3.12)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Pull in Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico-gba-cart C CXX)

# Initialise the Pico SDK
pico_sdk_init()

# build gba ROM
include(ExternalProject)
ExternalProject_Add(gba-rom-build
    PREFIX gba
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/gba
    BINARY_DIR ${CMAKE_BINARY_DIR}/gba

    CMAKE_ARGS "-DCMAKE_TOOLCHAIN_FILE=${GBA_TOOLCHAIN_FILE}" -DCMAKE_BUILD_TYPE=Release
    BUILD_ALWAYS TRUE

    INSTALL_COMMAND ""
)

add_custom_command(
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/gba/
    OUTPUT gba-rom.o
    COMMAND ${CMAKE_OBJCOPY} -I binary -O elf32-littlearm -B armv6s-m gba-rom.gba ${CMAKE_BINARY_DIR}/gba-rom.o
    DEPENDS ${CMAKE_BINARY_DIR}/gba/gba-rom.gba
)

# Add executable. Default name is the project name, version 0.1

add_executable(pico-gba-cart
    main.cpp
    gba-rom.o
)

pico_set_program_name(pico-gba-cart "pico-gba-cart")
pico_set_program_version(pico-gba-cart "0.1")

pico_enable_stdio_uart(pico-gba-cart 0)
pico_enable_stdio_usb(pico-gba-cart 0)

target_compile_options(pico-gba-cart PRIVATE -Wall)
target_link_libraries(pico-gba-cart pico_stdlib hardware_dma hardware_interp hardware_pio)

pico_generate_pio_header(pico-gba-cart ${CMAKE_CURRENT_LIST_DIR}/gba-cart.pio)

pico_add_extra_outputs(pico-gba-cart)

