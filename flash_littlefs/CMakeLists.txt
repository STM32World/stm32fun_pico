# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(PICO_PLATFORM rp2350)

# custom board headers
set(PICO_BOARD_HEADER_DIRS "${CMAKE_SOURCE_DIR}/boards/include" CACHE STRING "Extra board header dirs")
set(PICO_BOARD sl_mcu_rp2354 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK
include(pico_sdk_import.cmake)

project(main C CXX ASM)
pico_sdk_init()

add_executable(main
    main.c
)

# Limit application code size parameters to 1MB
set_target_properties(main PROPERTIES
    LINK_FLAGS "-Wl,--defsym,PICO_FLASH_SIZE_BYTES=0x100000"
)

target_compile_options(main PRIVATE
    -Ofast
    -ffunction-sections 
    -fdata-sections
)

target_link_libraries(
    main 
    pico_stdlib 
    pico_multicore
)

pico_enable_stdio_uart(main 1)
pico_enable_stdio_usb(main 0)

# ====================================================================================
# STANDALONE LITTLEFS BINARY GENERATION
# ====================================================================================
find_program(MKLITTLEFS_BIN mklittlefs)

if(NOT MKLITTLEFS_BIN)
    message(WARNING "mklittlefs utility not found on system path!")
else()
    set(FS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fs")
    set(FS_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/littlefs.bin")
    
    add_custom_command(
        OUTPUT ${FS_IMAGE}
        COMMAND ${MKLITTLEFS_BIN} -c ${FS_DIR} -s 1048576 -b 4096 -p 256 ${FS_IMAGE}
        DEPENDS ${FS_DIR}
        COMMENT "Generating Standalone LittleFS binary image..."
    )

    add_custom_target(generate_fs ALL DEPENDS ${FS_IMAGE})
    add_dependencies(main generate_fs)
endif()
# ====================================================================================

pico_add_extra_outputs(main)