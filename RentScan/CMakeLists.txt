cmake_minimum_required(VERSION 3.20.0)

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