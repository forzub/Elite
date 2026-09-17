cmake_minimum_required(VERSION 3.20)
project(EliteNavigationLocalBenchmark LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../src/world/navigation/local
                 ${CMAKE_CURRENT_BINARY_DIR}/navigation_local_lib)

add_executable(navigation_local_benchmark
    main.cpp
)

target_link_libraries(navigation_local_benchmark PRIVATE EliteNavigationLocal)
target_compile_features(navigation_local_benchmark PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(navigation_local_benchmark PRIVATE /W4 /permissive-)
else()
    target_compile_options(navigation_local_benchmark PRIVATE -Wall -Wextra -Wpedantic)
endif()
