cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rentscan_gateway_device)

# Include directories
include_directories(include)
include_directories(../common/include)

# Sources for this application
target_sources(app PRIVATE
  src/main.c
  src/ble_central.c
  src/gateway_service.c
)

# Add Kconfig overlay
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Configuration
target_compile_definitions(app PRIVATE
  -DCONFIG_RENTSCAN_GATEWAY_DEVICE=1) 