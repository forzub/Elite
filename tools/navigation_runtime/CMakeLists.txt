cmake_minimum_required(VERSION 3.20)

project(NavigationRuntimeViewer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(
    ELITE_SOURCE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE
)

find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(GLM_INCLUDE_DIR glm/glm.hpp REQUIRED)

add_executable(
    navigation_runtime_viewer
    NavigationRuntimeViewer.cpp
    NavigationTrace.cpp
    "${ELITE_SOURCE_ROOT}/glad/src/gl.c"
)

target_include_directories(
    navigation_runtime_viewer
    PRIVATE
        "${ELITE_SOURCE_ROOT}/glad"
        "${GLM_INCLUDE_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}"
)

target_link_libraries(
    navigation_runtime_viewer
    PRIVATE
        nlohmann_json::nlohmann_json
)

if(TARGET glfw)
    target_link_libraries(navigation_runtime_viewer PRIVATE glfw)
elseif(TARGET glfw3::glfw)
    target_link_libraries(navigation_runtime_viewer PRIVATE glfw3::glfw)
elseif(TARGET glfw3)
    target_link_libraries(navigation_runtime_viewer PRIVATE glfw3)
else()
    message(FATAL_ERROR "glfw3 package found but no supported GLFW target is available")
endif()

if(MSVC)
    target_compile_options(navigation_runtime_viewer PRIVATE /W4 /permissive-)
else()
    target_compile_options(navigation_runtime_viewer PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(
    navigation_runtime_viewer
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
