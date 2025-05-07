# Main CMakeLists.txt for RentScan application

cmake_minimum_required(VERSION 3.20.0)

# Include Zephyr's boilerplate
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Enable testing if requested
option(WITH_TESTING "Build with unit tests" OFF)

# Check what we're building
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/main_device AND IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/main_device)
    add_subdirectory(main_device)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gateway_device AND IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gateway_device)
    add_subdirectory(gateway_device)
endif()

if(WITH_TESTING)
    add_subdirectory(tests)
endif()

# Add common code
add_subdirectory(common)

# This top-level CMakeLists.txt is used to allow west to build either
# the main device or gateway device depending on the specified directory.
# 
# To build the main device:
#   west build -p -b nrf52840dk_nrf52840 main_device
#
# To build the gateway device:
#   west build -p -b nrf52840dk_nrf52840 gateway_device
#
# This file doesn't do anything specific as the actual builds are
# handled by the CMakeLists.txt files in each subdirectory.

# Print a helpful message
message(STATUS "RentScan - Wireless NFC Tag Rental System")
message(STATUS "To build a specific component, specify 'main_device' or 'gateway_device' as the source directory.")
message(STATUS "Example: west build -p -b nrf52840dk_nrf52840 main_device") 