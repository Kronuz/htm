cmake_minimum_required(VERSION 3.14)
project(htm LANGUAGES CXX)

include(FetchContent)

# --- Dependency: cartesian ---
# htm.h and constraint.h include "cartesian.h" for the Cartesian type (and the
# PI_HALF / DBL_TOLERANCE constants). That header lives in the sibling cartesian
# library, pulled in here so the include resolves. It is pinned to a commit so
# that builds are reproducible; the commit is the cartesian tip after the
# std::hash<Cartesian> equality fix.
FetchContent_Declare(
  cartesian
  GIT_REPOSITORY https://github.com/Kronuz/cartesian.git
  GIT_TAG        05cab81fe803532613e05fba65a37a8c14268cf7
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

    # The same, with getId forced onto the form that rounds each product behind an
    # asm: the form it falls back to when a compiler fuses despite the pragma.
    add_library(htm_rounded STATIC htm.cc constraint.cc)
    target_include_directories(htm_rounded PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_features(htm_rounded PUBLIC cxx_std_20)
    target_compile_options(htm_rounded PRIVATE ${HTM_FMA_FLAGS})
    target_compile_definitions(htm_rounded PRIVATE HTM_FORCE_ROUNDED)
    target_link_libraries(htm_rounded PUBLIC cartesian::cartesian)
    add_executable(htm_rounded_test test/diff_test.cc)
    target_link_libraries(htm_rounded_test PRIVATE htm_rounded)
    add_test(NAME htm_rounded COMMAND htm_rounded_test)
  endif()

  # Benchmark (not part of ctest). With HTM_BENCH_COMPARE it also times the SDSS HTM
  # library (the C++ library of Kunszt, Szalay and Thakar, which names the same
  # trixels) and H3, fetched at pinned versions and built with the same compiler and
  # flags; with HTM_BENCH_S2 it also times Google S2, which needs abseil and OpenSSL
  # installed. None of them is a dependency of the htm target.
  option(HTM_BENCH_COMPARE "Time the SDSS HTM library and H3 in htm_bench" OFF)
  option(HTM_BENCH_S2 "Time Google S2 in htm_bench (needs abseil and OpenSSL)" OFF)
  add_executable(htm_bench bench/bench.cc)
  target_link_libraries(htm_bench PRIVATE htm)

  if(HTM_BENCH_COMPARE)
    # SDSS HTM sources as vendored by gkhtm, pinned to a commit. The sources carry the
    # Johns Hopkins University copyright header; they are fetched for the bench and not
    # redistributed. gEpsilon (1e-15) in SpatialGeneral.h is their tolerance, unchanged.
    FetchContent_Declare(
      sdss_htm
      GIT_REPOSITORY https://github.com/genghisken/gkhtm.git
      GIT_TAG        e319c997a98019302428ca5667b288ee9c869deb
    )
    FetchContent_MakeAvailable(sdss_htm)
    set(SDSS_DIR "${sdss_htm_SOURCE_DIR}/gkhtm/htm")
    add_library(sdss_htm STATIC
      "${SDSS_DIR}/src/SpatialIndex.cpp"
      "${SDSS_DIR}/src/SpatialVector.cpp"
      "${SDSS_DIR}/src/SpatialEdge.cpp"
      "${SDSS_DIR}/src/SpatialException.cpp")
    target_include_directories(sdss_htm PUBLIC "${SDSS_DIR}/include")
    target_compile_definitions(sdss_htm PUBLIC LINKAGE=)
    target_compile_options(sdss_htm PRIVATE -w)

    # H3 v4.5.0, a C library (the bench target needs C enabled to inherit its C standard).
    enable_language(C)
    set(BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
    set(BUILD_FILTERS OFF CACHE BOOL "" FORCE)
    set(BUILD_GENERATORS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(ENABLE_FORMAT OFF CACHE BOOL "" FORCE)
    set(ENABLE_LINTING OFF CACHE BOOL "" FORCE)
    set(ENABLE_DOCS OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
      h3
      GIT_REPOSITORY https://github.com/uber/h3.git
      GIT_TAG        v4.5.0
    )
    FetchContent_MakeAvailable(h3)

    target_link_libraries(htm_bench PRIVATE sdss_htm h3)
    target_compile_definitions(htm_bench PRIVATE HTM_BENCH_SDSS HTM_BENCH_H3)
  endif()

  if(HTM_BENCH_S2)
    # Google S2 at a commit after v0.14.0 (the release does not build against abseil 20260817),
    # on the installed abseil, built as C++20 like the bench.
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CMAKE_CXX_STANDARD 20 CACHE STRING "" FORCE)
    FetchContent_Declare(
      s2geometry
      GIT_REPOSITORY https://github.com/google/s2geometry.git
      GIT_TAG        677c8cebcff11ba11accc0b02005bc214e06e9fd
    )
    FetchContent_MakeAvailable(s2geometry)
    target_link_libraries(htm_bench PRIVATE s2)
    target_compile_definitions(htm_bench PRIVATE HTM_BENCH_S2)
  endif()

  # A runnable tour of the library (not a test): build & run ./build/htm_demo
  add_executable(htm_demo examples/demo.cc)
  target_link_libraries(htm_demo PRIVATE htm)
endif()
