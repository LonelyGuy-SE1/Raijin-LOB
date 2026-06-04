cmake_minimum_required(VERSION 3.14)

project(RaijinLOB CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-error=attributes")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

set(RAIJIN_PGO_MODE "none" CACHE STRING "PGO mode: none, generate, use")

if(ipo_supported AND CMAKE_BUILD_TYPE STREQUAL "Release" AND RAIJIN_PGO_MODE STREQUAL "none")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()

include_directories(
    include
    third_party/json/include
)

add_library(core_objects OBJECT
    src/core/config.cpp
    src/core/order_pool.cpp
    src/core/limit_order_book.cpp
)

add_executable(raijin_engine src/main.cpp)
target_link_libraries(raijin_engine PRIVATE core_objects)

add_subdirectory(third_party/googletest)

include(CTest)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "tests/*.cpp")
add_executable(raijin_tests ${TEST_SOURCES})
target_link_libraries(raijin_tests PRIVATE core_objects GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(raijin_tests)

set(BENCHMARK_ENABLE_RTTI ON CACHE BOOL "Enable RTTI in Google Benchmark" FORCE)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark tests" FORCE)

add_subdirectory(third_party/benchmark)

add_executable(raijin_benchmarks benchmarks/lob_microbenchmarks.cpp)
target_link_libraries(raijin_benchmarks PRIVATE core_objects benchmark::benchmark)

add_executable(raijin_latency_histograms benchmarks/lob_latency_histograms.cpp)
target_link_libraries(raijin_latency_histograms PRIVATE core_objects benchmark::benchmark)

if(RAIJIN_PGO_MODE STREQUAL "generate")
    target_compile_options(core_objects PRIVATE -fprofile-generate)
    target_link_options(raijin_engine PRIVATE -fprofile-generate)
    target_link_options(raijin_benchmarks PRIVATE -fprofile-generate)
    target_link_options(raijin_latency_histograms PRIVATE -fprofile-generate)
endif()

if(RAIJIN_PGO_MODE STREQUAL "use")
    target_compile_options(core_objects PRIVATE -fprofile-use -fprofile-correction)
    target_link_options(raijin_engine PRIVATE -fprofile-use)
    target_link_options(raijin_benchmarks PRIVATE -fprofile-use)
    target_link_options(raijin_latency_histograms PRIVATE -fprofile-use)
endif()
