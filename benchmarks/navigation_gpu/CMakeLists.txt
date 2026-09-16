cmake_minimum_required(VERSION 3.16)

project(EliteNavigationGpuBenchmark LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(ELITE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

find_package(glfw3 CONFIG REQUIRED)

add_executable(navigation_gpu_benchmark
    main.cpp
    "${ELITE_ROOT}/glad/src/gl.c"
)

target_include_directories(navigation_gpu_benchmark PRIVATE
    "${ELITE_ROOT}"
    "${ELITE_ROOT}/glad/include"
)

target_link_libraries(navigation_gpu_benchmark PRIVATE glfw)

if(WIN32)
    target_link_libraries(navigation_gpu_benchmark PRIVATE opengl32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(navigation_gpu_benchmark PRIVATE OpenGL::GL)
endif()
