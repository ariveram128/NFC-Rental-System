# RentScan v3 Firmware
# CMakeLists.txt

cmake_minimum_required(VERSION 3.20.0)

# Find the Zephyr package
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Set up the project
project(rentscan_v3)

# Add source files
target_sources(app PRIVATE
  src/main.c
  src/nfc_handler.c
  src/rental_service.c
  src/rental_data.c
  src/storage_manager.c
  include/compat/nrf_fstorage_impl.c
)

# Include directories
target_include_directories(app PRIVATE
  include
  include/compat
)

# Preprocessor definitions
target_compile_definitions(app PRIVATE
  NRF52840_XXAA
  ZEPHYR_COMPAT_LAYER
)

# Compiler flags
target_compile_options(app PRIVATE
  -Wall
  -Werror
)

# Add optional zephyr_linker_script() if needed for custom memory layout
# zephyr_linker_script_ifdef(CONFIG_CUSTOM_LINKER_SCRIPT custom_linker.ld)
