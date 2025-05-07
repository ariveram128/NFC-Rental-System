cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rentscan_unit_tests)

# Include headers from application and common
zephyr_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../main_application/include
)

# Add test sources
target_sources(app PRIVATE
  test_rental_logic.c
)
