cmake_minimum_required(VERSION 3.20.0)

# Find Zephyr package
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rent_scan)

# Application sources
target_sources(app PRIVATE
    src/main.c
    src/nfc_handler.c
    src/ble_handler.c
    src/rental_logic.c
)

# Include directories
target_include_directories(app PRIVATE include)

