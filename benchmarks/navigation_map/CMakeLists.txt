cmake_minimum_required(VERSION 3.20)
project(EliteNavigationMapBenchmark LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../src/world/navigation/map
                 ${CMAKE_CURRENT_BINARY_DIR}/navigation_map_lib)

add_executable(navigation_map_benchmark
    main.cpp
)

target_link_libraries(navigation_map_benchmark PRIVATE EliteNavigationMap)
target_compile_features(navigation_map_benchmark PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(navigation_map_benchmark PRIVATE /W4 /permissive-)
else()
    target_compile_options(navigation_map_benchmark PRIVATE -Wall -Wextra -Wpedantic)
endif()
