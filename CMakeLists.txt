cmake_minimum_required(VERSION 3.10)
project(pmem_bench)

include(FindPkgConfig)
include(CheckIncludeFileCXX)
include_directories(include)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

find_package(OpenMP)
set(CMAKE_C_FLAGS "-g -O3 --fast-math")
set(CMAKE_CXX_FLAGS "-g -O3 --fast-math")
pkg_check_modules(libpmem REQUIRED IMPORTED_TARGET)

#Queue throughput
add_executable(test_spsc_exec src/test_spsc.cpp)
target_compile_options(test_spsc_exec PUBLIC "${OpenMP_CXX_FLAGS}")
target_link_libraries(test_spsc_exec "${OpenMP_CXX_FLAGS}" pmem)

#I/O throughput
add_executable(test_bw_exec src/max_bw.cpp)
target_compile_options(test_bw_exec PUBLIC "${OpenMP_CXX_FLAGS}")
target_link_libraries(test_bw_exec "${OpenMP_CXX_FLAGS}")
target_link_libraries(test_bw_exec "${OpenMP_CXX_FLAGS}" pmem)