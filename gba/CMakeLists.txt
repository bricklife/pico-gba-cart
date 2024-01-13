cmake_minimum_required(VERSION 3.18)

set(CMAKE_C_STANDARD 11)

project(gba-rom C)

gba_add_library_subdirectory(rom seven)

add_executable(gba-rom
    main.c
)
set_target_properties(gba-rom PROPERTIES SUFFIX ".elf")

target_compile_options(gba-rom PRIVATE -ffunction-sections -fdata-sections -mthumb)
target_include_directories(gba-rom PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../)
target_link_libraries(gba-rom PRIVATE rom seven)
target_link_options(gba-rom PRIVATE -Wl,--gc-sections)

# make some space after the header
target_compile_definitions(rom PRIVATE -D__ROM_START_PADDING__=32)

gba_target_objcopy(gba-rom FIX_HEADER GAME_CODE "CRPE")
