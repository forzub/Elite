cmake_minimum_required(VERSION 3.20)
project(PresentationPipelineTests LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(
    ELITE_SOURCE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE
)

add_executable(
    presentation_pipeline_tests
    PresentationPipelineTests.cpp
)

target_include_directories(
    presentation_pipeline_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(presentation_pipeline_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(presentation_pipeline_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(
    NAME presentation_pipeline
    COMMAND presentation_pipeline_tests
)

set_tests_properties(
    presentation_pipeline
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
