# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Find external Zephyr project
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Add main project target
project(rentscan_test_loopback)

# Include directories from both main_device and gateway_device
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../main_device/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../gateway_device/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../main_application/include
)

# Only include the loopback app sources when not building ZTEST tests
if (NOT CONFIG_ZTEST)
  target_sources(app PRIVATE
    src/test_loopback.c
  )
endif()

# Mock implementations always compile
target_sources(app PRIVATE
  src/mock_ble_service.c
  src/mock_ble_central.c
)