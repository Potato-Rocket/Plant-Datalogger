# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(datalogger C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

file(GLOB_RECURSE SOURCES "src/*.c")

add_executable(datalogger ${SOURCES})

pico_set_program_name(datalogger "Plant Datalogger")
pico_set_program_version(datalogger "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(datalogger 0)
pico_enable_stdio_usb(datalogger 1)

# Add the standard library to the build
target_link_libraries(datalogger
        pico_stdlib)

# Add third party libraries
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/pico_dht/dht)

# Add the standard include files to the build
target_include_directories(datalogger PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/lib/pico_dht/dht/include
        )

# Add any user requested libraries
target_link_libraries(datalogger
        hardware_rtc
        hardware_adc
        pico_cyw43_arch_lwip_threadsafe_background
        dht
        )

add_custom_command(TARGET datalogger
    POST_BUILD
    COMMAND echo Memory usage report:
    COMMAND arm-none-eabi-size datalogger.elf
)

pico_add_extra_outputs(datalogger)
