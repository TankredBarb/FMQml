cmake_minimum_required(VERSION 3.21)

project(WinsxsScanBench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC OFF)

find_package(Qt6 6.7 REQUIRED COMPONENTS Core)

add_executable(winsxs_scan_bench
    main.cpp
)

target_link_libraries(winsxs_scan_bench PRIVATE Qt6::Core)

if(WIN32)
    target_compile_definitions(winsxs_scan_bench PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
