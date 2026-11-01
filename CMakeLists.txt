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
  GIT_TAG        79ed663bf7b6c2481a314070dd84bc18e5ef7df8
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

  # Differential test of getId/getTrixelName against the IEEE-754 reference
  # (test/ieee.h), the golden ids, and a frozen copy of the original
  # Cartesian-object implementation.
  add_executable(htm_diff_test test/diff_test.cc)
  target_link_libraries(htm_diff_test PRIVATE htm)
  add_test(NAME htm_diff COMMAND htm_diff_test)

  # The ids must not depend on the compiler's floating-point contraction. The
  # same test runs against a second build of htm.cc with contraction allowed
  # everywhere (-ffp-contract=fast, and -mfma on x86 when this machine runs it),
  # which is where a fused multiply-add can change an id.
  include(CheckCXXCompilerFlag)
  include(CheckCXXSourceRuns)
  check_cxx_compiler_flag(-ffp-contract=fast HTM_HAVE_FP_CONTRACT_FAST)
  if(HTM_HAVE_FP_CONTRACT_FAST)
    set(HTM_FMA_FLAGS -ffp-contract=fast)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
      set(CMAKE_REQUIRED_FLAGS "-mfma")
      check_cxx_source_runs("
        #include <cmath>
        int main(int argc, char**) { volatile double a = 1.0 + argc; return std::fma(a, a, a) == 6.0 ? 0 : 1; }
      " HTM_HAVE_FMA)
      unset(CMAKE_REQUIRED_FLAGS)
      if(HTM_HAVE_FMA)
        list(APPEND HTM_FMA_FLAGS -mfma)
      endif()
    endif()
    add_library(htm_fma STATIC htm.cc constraint.cc)
    target_include_directories(htm_fma PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_features(htm_fma PUBLIC cxx_std_20)
    target_compile_options(htm_fma PRIVATE ${HTM_FMA_FLAGS})
    target_link_libraries(htm_fma PUBLIC cartesian::cartesian)
    add_executable(htm_fma_test test/diff_test.cc)
    target_link_libraries(htm_fma_test PRIVATE htm_fma)
    add_test(NAME htm_fma COMMAND htm_fma_test)
  endif()

  # Benchmark against the frozen original (not part of ctest).
  add_executable(htm_bench bench/bench.cc)
  target_link_libraries(htm_bench PRIVATE htm)

  # A runnable tour of the library (not a test): build & run ./build/htm_demo
  add_executable(htm_demo examples/demo.cc)
  target_link_libraries(htm_demo PRIVATE htm)
endif()
