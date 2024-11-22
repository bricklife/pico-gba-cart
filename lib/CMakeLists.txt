function(convert_bin2c input_file output_file c_var_name)
  file(READ ${input_file} BIN_HEX HEX)
  file(SIZE ${input_file} BIN_LEN)

  # Wrap lines per 16bytes
  string(REPEAT ".." 16 REGEX_PAT)
  string(REGEX REPLACE "(${REGEX_PAT})" "\\1\n" BIN_HEX_WRAP ${BIN_HEX})
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " BIN_ARRAY ${BIN_HEX_WRAP})

  # C format
  set(C_H_INC "#include <stdint.h>")
  set(C_H_DAT "static uint8_t ${c_var_name}[] = {\n${BIN_ARRAY}\n};")

  # Generate header
  file(WRITE ${output_file} "${C_H_INC}\n\n${C_H_DAT}\n")
endfunction()

convert_bin2c(${CMAKE_CURRENT_LIST_DIR}/../gba/rom.gba ${CMAKE_CURRENT_LIST_DIR}/rom.h _binary_gba_rom_gba_start)

add_library(gbacart STATIC
    gbacart.cpp
)

target_link_libraries(gbacart pico_stdlib hardware_dma hardware_pio)
target_include_directories(gbacart PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

pico_generate_pio_header(gbacart ${CMAKE_CURRENT_LIST_DIR}/gba-cart.pio)