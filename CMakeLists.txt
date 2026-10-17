cmake_minimum_required(VERSION 3.14)
project(htm LANGUAGES CXX)

include(FetchContent)

# --- Dependency: cartesian ---
# htm.h and constraint.h include "cartesian.h" for the Cartesian type (and the
# PI_HALF / DBL_TOLERANCE constants). That header lives in the sibling cartesian
# library, pulled in here so the include resolves. We track its tip with
# GIT_TAG main, like the rest of the family.
FetchContent_Declare(
  cartesian
  GIT_REPOSITORY https://github.com/Kronuz/cartesian.git
  GIT_TAG        34bf069089768d1e97bdfc2a390739f15590775b
)
FetchContent_MakeAvailable(cartesian)

# --- Library target ---
# htm is not header-only: htm.cc carries the trixel naming/id/range/set-operation
# code and constraint.cc carries the Constraint spherical-cap primitive, so both
# must be compiled and linked. The headers alone only declare the API.
add_library(htm htm.cc constraint.cc)
target_include_directories(htm PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(htm PUBLIC cxx_std_20)

# Link cartesian so "cartesian.h" is on the include path. PUBLIC because htm.h
# and constraint.h expose Cartesian in their interface, so consumers need it too.
target_link_libraries(htm PUBLIC cartesian::cartesian)

# Alias so consumers can link `htm::htm` whether they add this via
# add_subdirectory or FetchContent.
add_library(htm::htm ALIAS htm)

# Only build the test and demo when this repo is the top-level project, so
# consumers (e.g. via FetchContent) don't build them.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(htm_test test/test.cc)
  target_link_libraries(htm_test PRIVATE htm)
  add_test(NAME htm COMMAND htm_test)

  # A runnable tour of the library (not a test): build & run ./build/htm_demo
  add_executable(htm_demo examples/demo.cc)
  target_link_libraries(htm_demo PRIVATE htm)
endif()
