cmake_minimum_required(VERSION 3.20.0)

# Zephyr project
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rentscan_v3_firmware)

# Application sources
target_sources(app PRIVATE
  src/main.c
  src/nfc_handler.c
  src/rental_service.c
  src/rental_data.c
  src/storage_manager.c
  include/compat/nrf_fstorage_impl.c
)

# Include directories
target_include_directories(app PRIVATE include include/compat)
