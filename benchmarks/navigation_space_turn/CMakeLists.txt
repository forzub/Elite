cmake_minimum_required(VERSION 3.20)
project(EliteNavigationSpaceTurnBenchmark LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../src/world/navigation/space
                 ${CMAKE_CURRENT_BINARY_DIR}/navigation_space_lib)

add_executable(navigation_space_turn_benchmark
    main.cpp
)

target_link_libraries(navigation_space_turn_benchmark PRIVATE EliteNavigationSpace)
target_compile_features(navigation_space_turn_benchmark PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(navigation_space_turn_benchmark PRIVATE /W4 /permissive-)
else()
    target_compile_options(navigation_space_turn_benchmark PRIVATE -Wall -Wextra -Wpedantic)
endif()
