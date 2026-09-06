cmake_minimum_required(VERSION 3.16)
project(pxhash VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(PXHASH_BUILD_BENCHMARKS "Build benchmark executable" ON)
option(PXHASH_BUILD_EXAMPLES "Build example executables" ON)
option(PXHASH_BUILD_TESTS "Build test executable" ON)
option(PXHASH_ENABLE_AVX2 "Compile PXHash targets with AVX2 enabled" OFF)
option(PXHASH_NATIVE_ARCH "Compile local PXHash targets for the native CPU" OFF)

add_library(pxhash INTERFACE)
add_library(pxhash::pxhash ALIAS pxhash)

target_include_directories(pxhash INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(pxhash INTERFACE cxx_std_20)

function(pxhash_apply_warnings target_name)
  if (MSVC)
    target_compile_options(${target_name} PRIVATE /W4)
  else()
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

function(pxhash_apply_simd_options target_name visibility)
  if (PXHASH_ENABLE_AVX2)
    if (MSVC)
      target_compile_options(${target_name} ${visibility} /arch:AVX2)
    else()
      target_compile_options(${target_name} ${visibility} -mavx2)
    endif()
  endif()

  if (PXHASH_NATIVE_ARCH AND NOT MSVC)
    target_compile_options(${target_name} ${visibility} -march=native)
  endif()
endfunction()

pxhash_apply_simd_options(pxhash INTERFACE)

if (PXHASH_BUILD_BENCHMARKS)
  find_package(benchmark QUIET)
  find_package(absl QUIET CONFIG)

  if (benchmark_FOUND)
    message(STATUS "Google Benchmark found: enabling pxhash_bench")
  else()
    message(WARNING
      "PXHASH_BUILD_BENCHMARKS is ON, but Google Benchmark was not found. "
      "The pxhash_bench target will not be generated. "
      "Install Google Benchmark or reconfigure with -DPXHASH_BUILD_BENCHMARKS=OFF.")
  endif()
endif()

if (PXHASH_BUILD_BENCHMARKS AND benchmark_FOUND)
  add_executable(pxhash_bench bench/pxhash_bench.cpp)
  target_link_libraries(pxhash_bench PRIVATE pxhash::pxhash benchmark::benchmark)

  if (absl_FOUND)
    target_link_libraries(pxhash_bench PRIVATE absl::flat_hash_map)
    target_compile_definitions(pxhash_bench PRIVATE HAVE_ABSL=1)
  else()
    target_compile_definitions(pxhash_bench PRIVATE HAVE_ABSL=0)
  endif()

  pxhash_apply_warnings(pxhash_bench)
endif()

if (PXHASH_BUILD_TESTS)
  add_executable(pxhash_tests tests/pxhash_test.cpp)
  target_link_libraries(pxhash_tests PRIVATE pxhash::pxhash)
  pxhash_apply_warnings(pxhash_tests)

  add_test(NAME pxhash_tests COMMAND pxhash_tests)
endif()

if (PXHASH_BUILD_EXAMPLES)
  add_executable(pxhash_example_basic examples/basic_usage.cpp)
  target_link_libraries(pxhash_example_basic PRIVATE pxhash::pxhash)
  pxhash_apply_warnings(pxhash_example_basic)

  add_executable(pxhash_example_custom_key examples/custom_key.cpp)
  target_link_libraries(pxhash_example_custom_key PRIVATE pxhash::pxhash)
  pxhash_apply_warnings(pxhash_example_custom_key)

  add_executable(pxhash_example_binary_persistence examples/binary_persistence.cpp)
  target_link_libraries(pxhash_example_binary_persistence PRIVATE pxhash::pxhash)
  pxhash_apply_warnings(pxhash_example_binary_persistence)
endif()

install(TARGETS pxhash EXPORT pxhashTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT pxhashTargets
  FILE pxhashTargets.cmake
  NAMESPACE pxhash::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pxhash
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/pxhashConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pxhashConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/pxhashConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pxhash
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/pxhashConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/pxhashConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pxhash
)
