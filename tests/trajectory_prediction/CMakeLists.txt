cmake_minimum_required(VERSION 3.20)

project(TrajectoryPredictorTests LANGUAGES CXX)

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
    trajectory_predictor_tests
    TrajectoryPredictorTests.cpp
    "${ELITE_SOURCE_ROOT}/src/game/navigation/TrajectoryPredictor.cpp"
)

target_include_directories(
    trajectory_predictor_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        trajectory_predictor_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        trajectory_predictor_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME trajectory_predictor
    COMMAND trajectory_predictor_tests
)

set_tests_properties(
    trajectory_predictor
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
