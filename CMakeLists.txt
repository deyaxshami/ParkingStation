cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Top-level build entry for the ParkingStation firmware. Board support, HAL
# sources, startup code, and include paths are pulled in from cmake/stm32cubemx.

# STM32CubeMX-generated code and the local application use C11 features.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Default to Debug when the user configures CMake without a preset.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# This name becomes the firmware target name and output file prefix.
set(CMAKE_PROJECT_NAME ParkingStation)

# Export compile_commands.json so clangd and other editors can index the firmware.
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# The STM32 startup file is assembly, while the application and HAL are C.
enable_language(C ASM)

# Empty executable target; STM32CubeMX sources are attached below.
add_executable(${CMAKE_PROJECT_NAME})

# Adds generated application files, HAL drivers, CMSIS includes, and BSP support.
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Avoid linking the host-side ob library if C++ files are added later.
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Link the STM32CubeMX interface/driver targets into the firmware image.
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
