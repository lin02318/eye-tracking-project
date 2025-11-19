# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.12)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pt_blinky_serial C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(	pt_blinky_serial
    		Protothreads_blinky_and_serial_1_4.c
        	)

# pull in common dependencies
target_link_libraries(pt_blinky_serial 
    pico_stdlib 
    pico_multicore 
    pico_sync
    hardware_sync
    hardware_clocks
    hardware_vreg
    hardware_clocks
    )

# if (PICO_CYW43_SUPPORTED)
#     target_link_libraries(blink 
#     pico_cyw43_arch_none
#     )
# endif()

# create map/bin/hex file etc.
pico_add_extra_outputs(pt_blinky_serial)

add_compile_options(-O3)
