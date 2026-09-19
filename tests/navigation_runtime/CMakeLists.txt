cmake_minimum_required(VERSION 3.20)

project(NavigationRuntimeControlTests LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Production root CMake enables GLM experimental extensions globally because
# Ship/geometry headers legitimately include glm/gtx/*. The isolated runtime
# harness must mirror that compile contract instead of depending on incidental
# parent-project definitions.
add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

get_filename_component(
    ELITE_SOURCE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE
)

add_subdirectory(
    "${ELITE_SOURCE_ROOT}/src/world/navigation/control"
    "${CMAKE_BINARY_DIR}/elite_navigation_control"
)

add_subdirectory(
    "${ELITE_SOURCE_ROOT}/src/world/navigation/map"
    "${CMAKE_BINARY_DIR}/elite_navigation_map"
)

add_subdirectory(
    "${ELITE_SOURCE_ROOT}/src/world/navigation/local"
    "${CMAKE_BINARY_DIR}/elite_navigation_local"
)

add_subdirectory(
    "${ELITE_SOURCE_ROOT}/src/world/navigation/trajectory"
    "${CMAKE_BINARY_DIR}/elite_navigation_trajectory"
)

add_library(EliteNavigationRuntimeControl STATIC
    "${ELITE_SOURCE_ROOT}/src/game/navigation/NavigationRuntimeControlBridge.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/NpcNavigationIntentController.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/NavigationRuntimePlanner.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/TrajectoryFollower.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/ManeuverProgramSampler.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/ManeuverTrackingController.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/ManeuverPhaseGate.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/NavigationExecutionReplanPolicy.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/NavigationWorkScheduler.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/OrdinaryPhysicalManeuverCompiler.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/NavigationHitVolumeAdapter.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/DynamicMotionSystem.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/ship/controller/ManeuverDecisionController.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/shared/SharedShipPhysics.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/ship/ShipController.cpp"
)

target_include_directories(EliteNavigationRuntimeControl PUBLIC
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(EliteNavigationRuntimeControl PUBLIC cxx_std_17)
target_link_libraries(
    EliteNavigationRuntimeControl
    PUBLIC
        EliteNavigationControl
        EliteNavigationMap
        EliteNavigationLocal
        EliteNavigationTrajectory
)

add_executable(
    navigation_runtime_control_tests
    NavigationRuntimeControlTests.cpp
)

target_include_directories(navigation_runtime_control_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(navigation_runtime_control_tests PRIVATE cxx_std_17)
target_link_libraries(
    navigation_runtime_control_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(EliteNavigationRuntimeControl PRIVATE /W4 /permissive-)
    target_compile_options(navigation_runtime_control_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(EliteNavigationRuntimeControl PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(navigation_runtime_control_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(
    NAME navigation_runtime_control
    COMMAND navigation_runtime_control_tests
)

set_tests_properties(
    navigation_runtime_control
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    navigation_runtime_planner_tests
    NavigationRuntimePlannerTests.cpp
)

target_include_directories(navigation_runtime_planner_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(navigation_runtime_planner_tests PRIVATE cxx_std_17)
target_link_libraries(
    navigation_runtime_planner_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        navigation_runtime_planner_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        navigation_runtime_planner_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME navigation_runtime_planner
    COMMAND navigation_runtime_planner_tests
)

set_tests_properties(
    navigation_runtime_planner
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    maneuver_decision_controller_tests
    ManeuverDecisionControllerTests.cpp
)

target_include_directories(maneuver_decision_controller_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(maneuver_decision_controller_tests PRIVATE cxx_std_17)
target_link_libraries(
    maneuver_decision_controller_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        maneuver_decision_controller_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        maneuver_decision_controller_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME maneuver_decision_controller
    COMMAND maneuver_decision_controller_tests
)

set_tests_properties(
    maneuver_decision_controller
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    navigation_execution_replan_policy_tests
    NavigationExecutionReplanPolicyTests.cpp
)

target_include_directories(navigation_execution_replan_policy_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    navigation_execution_replan_policy_tests
    PRIVATE cxx_std_17
)
target_link_libraries(
    navigation_execution_replan_policy_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        navigation_execution_replan_policy_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        navigation_execution_replan_policy_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME navigation_execution_replan_policy
    COMMAND navigation_execution_replan_policy_tests
)

set_tests_properties(
    navigation_execution_replan_policy
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    maneuver_program_sampler_tests
    ManeuverProgramSamplerTests.cpp
)

target_include_directories(maneuver_program_sampler_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(maneuver_program_sampler_tests PRIVATE cxx_std_17)
target_link_libraries(
    maneuver_program_sampler_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        maneuver_program_sampler_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        maneuver_program_sampler_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME maneuver_program_sampler
    COMMAND maneuver_program_sampler_tests
)

set_tests_properties(
    maneuver_program_sampler
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    maneuver_tracking_controller_tests
    ManeuverTrackingControllerTests.cpp
)

target_include_directories(maneuver_tracking_controller_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(maneuver_tracking_controller_tests PRIVATE cxx_std_17)
target_link_libraries(
    maneuver_tracking_controller_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        maneuver_tracking_controller_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        maneuver_tracking_controller_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME maneuver_tracking_controller
    COMMAND maneuver_tracking_controller_tests
)

set_tests_properties(
    maneuver_tracking_controller
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    maneuver_corner_family_matrix_tests
    ManeuverCornerFamilyMatrixTests.cpp
)

target_include_directories(maneuver_corner_family_matrix_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    maneuver_corner_family_matrix_tests
    PRIVATE cxx_std_17
)
target_link_libraries(
    maneuver_corner_family_matrix_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        maneuver_corner_family_matrix_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        maneuver_corner_family_matrix_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME maneuver_corner_family_matrix
    COMMAND maneuver_corner_family_matrix_tests
)

set_tests_properties(
    maneuver_corner_family_matrix
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    maneuver_rigid_body_corridor_tests
    ManeuverRigidBodyCorridorTests.cpp
)

target_include_directories(maneuver_rigid_body_corridor_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    maneuver_rigid_body_corridor_tests
    PRIVATE cxx_std_17
)
target_link_libraries(
    maneuver_rigid_body_corridor_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        maneuver_rigid_body_corridor_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        maneuver_rigid_body_corridor_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME maneuver_rigid_body_corridor
    COMMAND maneuver_rigid_body_corridor_tests
)

set_tests_properties(
    maneuver_rigid_body_corridor
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    maneuver_corridor_matrix_tests
    ManeuverCorridorMatrixTests.cpp
)

target_include_directories(maneuver_corridor_matrix_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    maneuver_corridor_matrix_tests
    PRIVATE cxx_std_17
)
target_link_libraries(
    maneuver_corridor_matrix_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        maneuver_corridor_matrix_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        maneuver_corridor_matrix_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME maneuver_corridor_matrix
    COMMAND maneuver_corridor_matrix_tests
)

set_tests_properties(
    maneuver_corridor_matrix
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    maneuver_program_execution_lab_tests
    ManeuverProgramExecutionLabTests.cpp
)

target_include_directories(maneuver_program_execution_lab_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    maneuver_program_execution_lab_tests
    PRIVATE cxx_std_17
)
target_link_libraries(
    maneuver_program_execution_lab_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        maneuver_program_execution_lab_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        maneuver_program_execution_lab_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME maneuver_program_execution_lab
    COMMAND maneuver_program_execution_lab_tests
)

set_tests_properties(
    maneuver_program_execution_lab
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    ordinary_physical_maneuver_compiler_tests
    OrdinaryPhysicalManeuverCompilerTests.cpp
)

target_include_directories(ordinary_physical_maneuver_compiler_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    ordinary_physical_maneuver_compiler_tests
    PRIVATE cxx_std_17
)
target_link_libraries(
    ordinary_physical_maneuver_compiler_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        ordinary_physical_maneuver_compiler_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        ordinary_physical_maneuver_compiler_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME ordinary_physical_maneuver_compiler
    COMMAND ordinary_physical_maneuver_compiler_tests
)

set_tests_properties(
    ordinary_physical_maneuver_compiler
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    navigation_work_scheduler_tests
    NavigationWorkSchedulerTests.cpp
)

target_include_directories(navigation_work_scheduler_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(navigation_work_scheduler_tests PRIVATE cxx_std_17)
target_link_libraries(
    navigation_work_scheduler_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        navigation_work_scheduler_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        navigation_work_scheduler_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME navigation_work_scheduler
    COMMAND navigation_work_scheduler_tests
)

set_tests_properties(
    navigation_work_scheduler
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    navigation_replication_truth_tests
    NavigationReplicationTruthTests.cpp
)

target_include_directories(navigation_replication_truth_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(navigation_replication_truth_tests PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(
        navigation_replication_truth_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        navigation_replication_truth_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME navigation_replication_truth
    COMMAND navigation_replication_truth_tests
)

set_tests_properties(
    navigation_replication_truth
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)


add_executable(
    physical_maneuver_horizon_tests
    PhysicalManeuverHorizonTests.cpp
)

target_include_directories(physical_maneuver_horizon_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(physical_maneuver_horizon_tests PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(physical_maneuver_horizon_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(physical_maneuver_horizon_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(
    NAME physical_maneuver_horizon
    COMMAND physical_maneuver_horizon_tests
)

set_tests_properties(
    physical_maneuver_horizon
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
