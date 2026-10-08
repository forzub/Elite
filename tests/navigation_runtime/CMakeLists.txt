cmake_minimum_required(VERSION 3.20)

project(NavigationRuntimeControlTests LANGUAGES CXX)

include(CTest)

find_package(nlohmann_json CONFIG REQUIRED)

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

include("${ELITE_SOURCE_ROOT}/cmake/EliteRuckigNavigation.cmake")
elite_add_ruckig_navigation(
    EliteNavigationRuckig
    "${ELITE_SOURCE_ROOT}"
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
    "${ELITE_SOURCE_ROOT}/src/game/navigation/NominalRoutePlanner.cpp"
    "${ELITE_SOURCE_ROOT}/src/world/navigation/GeometricPathPlanner.cpp"
    "${ELITE_SOURCE_ROOT}/src/world/navigation/TrajectoryGenerator.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/DockingAdvisoryPlanner.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/planner/RoutePlanner.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/TrajectoryFollower.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/ManeuverProgramSampler.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/ManeuverTrackingController.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/ManeuverPhaseGate.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/NavigationExecutionReplanPolicy.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/NavigationWorkScheduler.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/OrdinaryPhysicalManeuverCompiler.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/PhysicalManeuverSearchCoordinator.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/NavigationHitVolumeAdapter.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/HubSemanticAnchorCatalog.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/NavigationVolumeCatalog.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/traffic/TrafficRouteGraph.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/traffic/TrafficRouteGraphCatalog.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/diagnostics/NavigationScenarioRunner.cpp"
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
        EliteNavigationRuckig
        nlohmann_json::nlohmann_json
)

add_executable(
    traffic_route_graph_tests
    TrafficRouteGraphTests.cpp
)

target_include_directories(traffic_route_graph_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(traffic_route_graph_tests PRIVATE cxx_std_17)
target_link_libraries(
    traffic_route_graph_tests
    PRIVATE EliteNavigationRuntimeControl nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(
        traffic_route_graph_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        traffic_route_graph_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME traffic_route_graph
    COMMAND traffic_route_graph_tests
)

set_tests_properties(
    traffic_route_graph
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
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
    EXCLUDE_FROM_ALL
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
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    maneuver_phase_gate_tests
    EXCLUDE_FROM_ALL
    ManeuverPhaseGateTests.cpp
)

target_include_directories(maneuver_phase_gate_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(maneuver_phase_gate_tests PRIVATE cxx_std_17)
target_link_libraries(
    maneuver_phase_gate_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        maneuver_phase_gate_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        maneuver_phase_gate_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME maneuver_phase_gate
    COMMAND maneuver_phase_gate_tests
)

set_tests_properties(
    maneuver_phase_gate
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    maneuver_tracking_controller_tests
    EXCLUDE_FROM_ALL
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
    EXCLUDE_FROM_ALL
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
    EXCLUDE_FROM_ALL
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
    EXCLUDE_FROM_ALL
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
    maneuver_fly_through_3d_tests
    EXCLUDE_FROM_ALL
    ManeuverFlyThrough3dTests.cpp
)

target_include_directories(maneuver_fly_through_3d_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    maneuver_fly_through_3d_tests
    PRIVATE cxx_std_17
)
target_link_libraries(
    maneuver_fly_through_3d_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        maneuver_fly_through_3d_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        maneuver_fly_through_3d_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME maneuver_fly_through_3d
    COMMAND maneuver_fly_through_3d_tests
)

set_tests_properties(
    maneuver_fly_through_3d
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    maneuver_speed_doctrine_matrix_tests
    EXCLUDE_FROM_ALL
    ManeuverSpeedDoctrineMatrixTests.cpp
)

target_include_directories(maneuver_speed_doctrine_matrix_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    maneuver_speed_doctrine_matrix_tests
    PRIVATE cxx_std_17
)
target_link_libraries(
    maneuver_speed_doctrine_matrix_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        maneuver_speed_doctrine_matrix_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        maneuver_speed_doctrine_matrix_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME maneuver_speed_doctrine_matrix
    COMMAND maneuver_speed_doctrine_matrix_tests
)

set_tests_properties(
    maneuver_speed_doctrine_matrix
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    maneuver_chained_limit_matrix_tests
    EXCLUDE_FROM_ALL
    ManeuverChainedLimitMatrixTests.cpp
)

target_include_directories(maneuver_chained_limit_matrix_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    maneuver_chained_limit_matrix_tests
    PRIVATE cxx_std_17
)
target_link_libraries(
    maneuver_chained_limit_matrix_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        maneuver_chained_limit_matrix_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        maneuver_chained_limit_matrix_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME maneuver_chained_limit_matrix
    COMMAND maneuver_chained_limit_matrix_tests
)

set_tests_properties(
    maneuver_chained_limit_matrix
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)


# Isolated navigation guidance layers. These are deliberately separate CTest
# targets so one responsibility can be frozen before another is modified.
foreach(guidance_test
    hull_pose_guidance
    hull_attitude_control
    route_speed_guidance
    course_capture_guidance
)
    if(guidance_test STREQUAL "hull_pose_guidance")
        set(guidance_source HullPoseGuidanceTests.cpp)
    elseif(guidance_test STREQUAL "hull_attitude_control")
        set(guidance_source HullAttitudeControlTests.cpp)
    elseif(guidance_test STREQUAL "route_speed_guidance")
        set(guidance_source RouteSpeedGuidanceTests.cpp)
    else()
        set(guidance_source CourseCaptureGuidanceTests.cpp)
    endif()

    add_executable(
        ${guidance_test}_tests
        ${guidance_source}
    )

    target_include_directories(
        ${guidance_test}_tests
        PRIVATE
            "${ELITE_SOURCE_ROOT}"
            "${ELITE_SOURCE_ROOT}/src"
    )

    target_compile_features(
        ${guidance_test}_tests
        PRIVATE cxx_std_17
    )

    if(MSVC)
        target_compile_options(
            ${guidance_test}_tests
            PRIVATE /W4 /permissive-
        )
    else()
        target_compile_options(
            ${guidance_test}_tests
            PRIVATE -Wall -Wextra -Wpedantic
        )
    endif()

    add_test(
        NAME ${guidance_test}
        COMMAND ${guidance_test}_tests
    )

    set_tests_properties(
        ${guidance_test}
        PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
    )
endforeach()

add_executable(
    hull_roll_alignment_contract_tests
    HullRollAlignmentContractTests.cpp
)

target_include_directories(
    hull_roll_alignment_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    hull_roll_alignment_contract_tests
    PRIVATE cxx_std_17
)

if(MSVC)
    target_compile_options(
        hull_roll_alignment_contract_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        hull_roll_alignment_contract_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME hull_roll_alignment_contract
    COMMAND hull_roll_alignment_contract_tests
)

set_tests_properties(
    hull_roll_alignment_contract
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    navigation_scenario_runner_tests
    NavigationScenarioRunnerTests.cpp
)

target_include_directories(
    navigation_scenario_runner_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    navigation_scenario_runner_tests
    PRIVATE cxx_std_17
)

target_link_libraries(
    navigation_scenario_runner_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        navigation_scenario_runner_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        navigation_scenario_runner_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME navigation_scenario_runner
    COMMAND navigation_scenario_runner_tests
)

set_tests_properties(
    navigation_scenario_runner
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    client_route_autopilot_tests
    ClientRouteAutopilotTests.cpp
)

target_include_directories(
    client_route_autopilot_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    client_route_autopilot_tests
    PRIVATE cxx_std_17
)

target_link_libraries(
    client_route_autopilot_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        client_route_autopilot_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        client_route_autopilot_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME client_route_autopilot
    COMMAND client_route_autopilot_tests
)

set_tests_properties(
    client_route_autopilot
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    navigation_v2_tunnel_proving_ground_tests
    NavigationV2TunnelProvingGroundTests.cpp
)

target_include_directories(
    navigation_v2_tunnel_proving_ground_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    navigation_v2_tunnel_proving_ground_tests
    PRIVATE cxx_std_17
)

target_link_libraries(
    navigation_v2_tunnel_proving_ground_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        navigation_v2_tunnel_proving_ground_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        navigation_v2_tunnel_proving_ground_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME navigation_v2_tunnel_proving_ground
    COMMAND navigation_v2_tunnel_proving_ground_tests
)

set_tests_properties(
    navigation_v2_tunnel_proving_ground
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    maneuver_program_execution_lab_tests
    EXCLUDE_FROM_ALL
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
    ship_propulsion_state_tests
    ShipPropulsionStateTests.cpp
)

target_include_directories(ship_propulsion_state_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    ship_propulsion_state_tests
    PRIVATE cxx_std_17
)

if(MSVC)
    target_compile_options(
        ship_propulsion_state_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        ship_propulsion_state_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME ship_propulsion_state
    COMMAND ship_propulsion_state_tests
)

set_tests_properties(
    ship_propulsion_state
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    physical_maneuver_search_coordinator_tests
    PhysicalManeuverSearchCoordinatorTests.cpp
)

target_include_directories(physical_maneuver_search_coordinator_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    physical_maneuver_search_coordinator_tests
    PRIVATE cxx_std_17
)
target_link_libraries(
    physical_maneuver_search_coordinator_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        physical_maneuver_search_coordinator_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        physical_maneuver_search_coordinator_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME physical_maneuver_search_coordinator
    COMMAND physical_maneuver_search_coordinator_tests
)

set_tests_properties(
    physical_maneuver_search_coordinator
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

add_executable(
    geometric_path_planner_tests
    GeometricPathPlannerTests.cpp
)

target_include_directories(
    geometric_path_planner_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    geometric_path_planner_tests
    PRIVATE cxx_std_17
)

target_link_libraries(
    geometric_path_planner_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        geometric_path_planner_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        geometric_path_planner_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME geometric_path_planner
    COMMAND geometric_path_planner_tests
)

set_tests_properties(
    geometric_path_planner
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(docking_advisory_tests
    DockingAdvisoryPlannerTests.cpp
    "${ELITE_SOURCE_ROOT}/src/game/navigation/NavigationWorldPredictor.cpp")
target_include_directories(docking_advisory_tests PRIVATE
    "${ELITE_SOURCE_ROOT}" "${ELITE_SOURCE_ROOT}/src")
target_compile_features(docking_advisory_tests PRIVATE cxx_std_17)
target_link_libraries(docking_advisory_tests PRIVATE EliteNavigationRuntimeControl)
add_test(NAME docking_advisory COMMAND docking_advisory_tests)

add_executable(
    route_planner_api_tests
    RoutePlannerApiTests.cpp
)
target_include_directories(route_planner_api_tests PRIVATE
    "${ELITE_SOURCE_ROOT}" "${ELITE_SOURCE_ROOT}/src")
target_compile_features(route_planner_api_tests PRIVATE cxx_std_17)
target_link_libraries(route_planner_api_tests PRIVATE EliteNavigationRuntimeControl)
add_test(NAME route_planner_api COMMAND route_planner_api_tests)

add_executable(
    docking_infrastructure_api_tests
    DockingInfrastructureApiTests.cpp
    "${ELITE_SOURCE_ROOT}/src/game/docking/traffic/DockTrafficController.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/docking/landing/DockLandingController.cpp"
)
target_include_directories(docking_infrastructure_api_tests PRIVATE
    "${ELITE_SOURCE_ROOT}" "${ELITE_SOURCE_ROOT}/src")
target_compile_features(docking_infrastructure_api_tests PRIVATE cxx_std_17)
add_test(NAME docking_infrastructure_api COMMAND docking_infrastructure_api_tests)

add_executable(
    navigation_hit_volume_adapter_tests
    NavigationHitVolumeAdapterTests.cpp
)
target_include_directories(
    navigation_hit_volume_adapter_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)
target_compile_features(
    navigation_hit_volume_adapter_tests
    PRIVATE cxx_std_17
)
target_link_libraries(
    navigation_hit_volume_adapter_tests
    PRIVATE EliteNavigationRuntimeControl
)
add_test(
    NAME navigation_hit_volume_adapter
    COMMAND navigation_hit_volume_adapter_tests
)

add_executable(
    nominal_route_planner_tests
    NominalRoutePlannerTests.cpp
)

target_include_directories(
    nominal_route_planner_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    nominal_route_planner_tests
    PRIVATE cxx_std_17
)

target_link_libraries(
    nominal_route_planner_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        nominal_route_planner_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        nominal_route_planner_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME nominal_route_planner
    COMMAND nominal_route_planner_tests
)

set_tests_properties(
    nominal_route_planner
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
        LABELS "navigation_stage1"
)


add_executable(
    accepted_maneuver_program_builder_tests
    AcceptedManeuverProgramBuilderTests.cpp
)

target_include_directories(
    accepted_maneuver_program_builder_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(
    accepted_maneuver_program_builder_tests
    PRIVATE cxx_std_17
)

target_link_libraries(
    accepted_maneuver_program_builder_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(
        accepted_maneuver_program_builder_tests
        PRIVATE /W4 /permissive-
    )
else()
    target_compile_options(
        accepted_maneuver_program_builder_tests
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_test(
    NAME accepted_maneuver_program_builder
    COMMAND accepted_maneuver_program_builder_tests
)

set_tests_properties(
    accepted_maneuver_program_builder
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)


# Legacy comparison/lab suite.
#
# These tests intentionally exercise retired execution seams such as
# TrajectoryFollower, NavigationRuntimeControlBridge or the old Assisted
# actuator helper. They remain useful as historical diagnostics/oracles, but
# they MUST NOT participate in the production PredictivePilot V2 acceptance
# gate or in the default navigation-runtime run.
set_tests_properties(
    maneuver_phase_gate
    maneuver_program_sampler
    maneuver_tracking_controller
    maneuver_program_execution_lab
    maneuver_corridor_matrix
    maneuver_rigid_body_corridor
    maneuver_corner_family_matrix
    maneuver_fly_through_3d
    maneuver_speed_doctrine_matrix
    maneuver_chained_limit_matrix
    navigation_runtime_planner
    PROPERTIES LABELS "legacy_navigation_lab"
)
