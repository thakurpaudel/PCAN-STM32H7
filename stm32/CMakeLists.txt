cmake_minimum_required(VERSION 3.22)

# Setup compiler settings for C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Setup compiler settings for C
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME PCAN)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for C++, C, and ASM languages
enable_language(CXX C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Add C++ specific compiler flags (ONLY for C++ files)
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
)

# Include user.cmake for additional components
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/user.cmake)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# CAN Mode Selection (CLASSIC or FD)
# Default to PCAN-USB Pro FD so the USB descriptor, endpoint layout and
# protocol source match what PEAK drivers/PCAN-View expect.
option(PCAN_CAN_FD_MODE "Enable CAN-FD mode (vs Classic CAN)" ON)
option(PCAN_DISCO_H745 "Build for STM32H745I-DISCO Board" OFF)

# Add project symbols (macros)
if(PCAN_DISCO_H745)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        USE_CPP_LOGGER
        STM32H745xx
        DISCO_H745
        # CAN mode configuration
        $<$<BOOL:${PCAN_CAN_FD_MODE}>:PCAN_PRO_FD>
        $<$<BOOL:${PCAN_CAN_FD_MODE}>:PCAN_CAN_FD_ENABLED=1>
        $<$<NOT:$<BOOL:${PCAN_CAN_FD_MODE}>>:PCAN_PRO>
        $<$<NOT:$<BOOL:${PCAN_CAN_FD_MODE}>>:PCAN_CLASSIC_MODE=1>
    )
else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        USE_CPP_LOGGER
        STM32H743XX
        # CAN mode configuration
        $<$<BOOL:${PCAN_CAN_FD_MODE}>:PCAN_PRO_FD>
        $<$<BOOL:${PCAN_CAN_FD_MODE}>:PCAN_CAN_FD_ENABLED=1>
        $<$<NOT:$<BOOL:${PCAN_CAN_FD_MODE}>>:PCAN_PRO>
        $<$<NOT:$<BOOL:${PCAN_CAN_FD_MODE}>>:PCAN_CLASSIC_MODE=1>
    )
endif()

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    # Add user defined libraries
)
