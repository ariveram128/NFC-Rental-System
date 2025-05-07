cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rentscan_main_device)

# Include directories
include_directories(include)
include_directories(../common/include)

# Nordic SDK includes
include_directories($ENV{ZEPHYR_BASE}/../nrf/include)
include_directories($ENV{ZEPHYR_BASE}/../nrfxlib/nfc/include)
include_directories($ENV{ZEPHYR_BASE}/include)
include_directories($ENV{ZEPHYR_BASE}/../modules/hal/nordic/nrfx/hal)

# Sources for this application
target_sources(app PRIVATE
  src/main.c
  src/nfc_handler.c
  src/ble_service.c
  src/rental_manager.c
)

# Add Kconfig overlay
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Configuration
target_compile_definitions(app PRIVATE
  -DCONFIG_RENTSCAN_MAIN_DEVICE=1
)