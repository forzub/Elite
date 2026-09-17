cmake_minimum_required(VERSION 3.20)

project(NavigationTrajectoryTests LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(
    ELITE_SOURCE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE
)

add_subdirectory(
    "${ELITE_SOURCE_ROOT}/src/world/navigation/trajectory"
    "${CMAKE_BINARY_DIR}/elite_navigation_trajectory"
)

add_executable(navigation_trajectory_passage_tests NavigationTrajectoryPassageTests.cpp)
target_include_directories(navigation_trajectory_passage_tests PRIVATE "${ELITE_SOURCE_ROOT}" "${ELITE_SOURCE_ROOT}/src")
target_compile_features(navigation_trajectory_passage_tests PRIVATE cxx_std_17)
target_link_libraries(navigation_trajectory_passage_tests PRIVATE EliteNavigationTrajectory)

add_executable(navigation_trajectory_gap_tests NavigationTrajectoryGapTests.cpp)
target_include_directories(navigation_trajectory_gap_tests PRIVATE "${ELITE_SOURCE_ROOT}" "${ELITE_SOURCE_ROOT}/src")
target_compile_features(navigation_trajectory_gap_tests PRIVATE cxx_std_17)
target_link_libraries(navigation_trajectory_gap_tests PRIVATE EliteNavigationTrajectory)

add_executable(navigation_trajectory_reachability_tests NavigationTrajectoryReachabilityTests.cpp)
target_include_directories(navigation_trajectory_reachability_tests PRIVATE "${ELITE_SOURCE_ROOT}" "${ELITE_SOURCE_ROOT}/src")
target_compile_features(navigation_trajectory_reachability_tests PRIVATE cxx_std_17)
target_link_libraries(navigation_trajectory_reachability_tests PRIVATE EliteNavigationTrajectory)

add_executable(navigation_trajectory_emergency_passage_tests NavigationTrajectoryEmergencyPassageTests.cpp)
target_include_directories(navigation_trajectory_emergency_passage_tests PRIVATE "${ELITE_SOURCE_ROOT}" "${ELITE_SOURCE_ROOT}/src")
target_compile_features(navigation_trajectory_emergency_passage_tests PRIVATE cxx_std_17)
target_link_libraries(navigation_trajectory_emergency_passage_tests PRIVATE EliteNavigationTrajectory)

add_executable(navigation_trajectory_continuous_passage_tests NavigationTrajectoryContinuousPassageTests.cpp)
target_include_directories(navigation_trajectory_continuous_passage_tests PRIVATE "${ELITE_SOURCE_ROOT}" "${ELITE_SOURCE_ROOT}/src")
target_compile_features(navigation_trajectory_continuous_passage_tests PRIVATE cxx_std_17)
target_link_libraries(navigation_trajectory_continuous_passage_tests PRIVATE EliteNavigationTrajectory)

add_executable(navigation_trajectory_emergency_contact_severity_tests NavigationTrajectoryEmergencyContactSeverityTests.cpp)
target_include_directories(navigation_trajectory_emergency_contact_severity_tests PRIVATE "${ELITE_SOURCE_ROOT}" "${ELITE_SOURCE_ROOT}/src")
target_compile_features(navigation_trajectory_emergency_contact_severity_tests PRIVATE cxx_std_17)
target_link_libraries(navigation_trajectory_emergency_contact_severity_tests PRIVATE EliteNavigationTrajectory)

add_executable(navigation_trajectory_moving_gap_tests NavigationTrajectoryMovingGapTests.cpp)
target_include_directories(navigation_trajectory_moving_gap_tests PRIVATE "${ELITE_SOURCE_ROOT}" "${ELITE_SOURCE_ROOT}/src")
target_compile_features(navigation_trajectory_moving_gap_tests PRIVATE cxx_std_17)
target_link_libraries(navigation_trajectory_moving_gap_tests PRIVATE EliteNavigationTrajectory)

add_executable(navigation_trajectory_moving_passage_tests NavigationTrajectoryMovingPassageTests.cpp)
target_include_directories(navigation_trajectory_moving_passage_tests PRIVATE "${ELITE_SOURCE_ROOT}" "${ELITE_SOURCE_ROOT}/src")
target_compile_features(navigation_trajectory_moving_passage_tests PRIVATE cxx_std_17)
target_link_libraries(navigation_trajectory_moving_passage_tests PRIVATE EliteNavigationTrajectory)

if(MSVC)
    foreach(target
        navigation_trajectory_passage_tests
        navigation_trajectory_gap_tests
        navigation_trajectory_reachability_tests
        navigation_trajectory_emergency_passage_tests
        navigation_trajectory_continuous_passage_tests
        navigation_trajectory_emergency_contact_severity_tests
        navigation_trajectory_moving_gap_tests
        navigation_trajectory_moving_passage_tests)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    endforeach()
else()
    foreach(target
        navigation_trajectory_passage_tests
        navigation_trajectory_gap_tests
        navigation_trajectory_reachability_tests
        navigation_trajectory_emergency_passage_tests
        navigation_trajectory_continuous_passage_tests
        navigation_trajectory_emergency_contact_severity_tests
        navigation_trajectory_moving_gap_tests
        navigation_trajectory_moving_passage_tests)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endforeach()
endif()

add_test(NAME navigation_trajectory_passage COMMAND navigation_trajectory_passage_tests)
add_test(NAME navigation_trajectory_gap COMMAND navigation_trajectory_gap_tests)
add_test(NAME navigation_trajectory_reachability COMMAND navigation_trajectory_reachability_tests)
add_test(NAME navigation_trajectory_emergency_passage COMMAND navigation_trajectory_emergency_passage_tests)
add_test(NAME navigation_trajectory_continuous_passage COMMAND navigation_trajectory_continuous_passage_tests)
add_test(NAME navigation_trajectory_emergency_contact_severity COMMAND navigation_trajectory_emergency_contact_severity_tests)
add_test(NAME navigation_trajectory_moving_gap COMMAND navigation_trajectory_moving_gap_tests)
add_test(NAME navigation_trajectory_moving_passage COMMAND navigation_trajectory_moving_passage_tests)

set_tests_properties(
    navigation_trajectory_passage
    navigation_trajectory_gap
    navigation_trajectory_reachability
    navigation_trajectory_emergency_passage
    navigation_trajectory_continuous_passage
    navigation_trajectory_emergency_contact_severity
    navigation_trajectory_moving_gap
    navigation_trajectory_moving_passage
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
