cmake_minimum_required(VERSION 3.20)

project(ArchitectureContractTests LANGUAGES CXX)

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
    architecture_contract_tests
    ArchitectureContractTests.cpp
    "${ELITE_SOURCE_ROOT}/src/game/client/MapTransitionController.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/DynamicMotionSystem.cpp"
)

target_include_directories(
    architecture_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        architecture_contract_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        architecture_contract_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME architecture_contracts
    COMMAND architecture_contract_tests
)

add_executable(
    runtime_policy_contract_tests
    RuntimePolicyContractTests.cpp
)

target_include_directories(
    runtime_policy_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        runtime_policy_contract_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        runtime_policy_contract_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME runtime_policy_contracts
    COMMAND runtime_policy_contract_tests
)



add_executable(
    hub_motion_lab_contract_tests
    HubMotionLabContractTests.cpp
)

target_include_directories(
    hub_motion_lab_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        hub_motion_lab_contract_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        hub_motion_lab_contract_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME hub_motion_lab_contracts
    COMMAND hub_motion_lab_contract_tests
)

set_tests_properties(
    architecture_contracts
    runtime_policy_contracts
    hub_motion_lab_contracts
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    local_predicted_presentation_contract_tests
    LocalPredictedPresentationContractTests.cpp
    "${ELITE_SOURCE_ROOT}/src/game/navigation/DynamicMotionSystem.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/shared/SharedShipPhysics.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/ship/ShipController.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/ship/physics/ShipImpulseSystem.cpp"
)

target_include_directories(
    local_predicted_presentation_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        local_predicted_presentation_contract_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        local_predicted_presentation_contract_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME local_predicted_presentation_contracts
    COMMAND local_predicted_presentation_contract_tests
)

set_tests_properties(
    local_predicted_presentation_contracts
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    player_spatial_domain_contract_tests
    PlayerSpatialDomainContractTests.cpp
    "${ELITE_SOURCE_ROOT}/src/game/navigation/PlayerSpatialDomainResolver.cpp"
)

target_include_directories(
    player_spatial_domain_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        player_spatial_domain_contract_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        player_spatial_domain_contract_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME player_spatial_domain_contracts
    COMMAND player_spatial_domain_contract_tests
)

set_tests_properties(
    player_spatial_domain_contracts
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    coordinate_frame_contract_tests
    CoordinateFrameContractTests.cpp
    "${ELITE_SOURCE_ROOT}/src/game/presentation/GalaxyNavigationPresentation.cpp"
)

target_include_directories(
    coordinate_frame_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        coordinate_frame_contract_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        coordinate_frame_contract_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME coordinate_frame_contracts
    COMMAND coordinate_frame_contract_tests
)

set_tests_properties(
    coordinate_frame_contracts
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    kinematic_frame_contract_tests
    KinematicFrameContractTests.cpp
)

target_include_directories(
    kinematic_frame_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        kinematic_frame_contract_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        kinematic_frame_contract_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME kinematic_frame_contracts
    COMMAND kinematic_frame_contract_tests
)

set_tests_properties(
    kinematic_frame_contracts
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    travel_frame_contract_tests
    TravelFrameContractTests.cpp
)

target_include_directories(
    travel_frame_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        travel_frame_contract_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        travel_frame_contract_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME travel_frame_contracts
    COMMAND travel_frame_contract_tests
)

set_tests_properties(
    travel_frame_contracts
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    local_flight_control_contract_tests
    LocalFlightControlContractTests.cpp
    "${ELITE_SOURCE_ROOT}/src/game/navigation/DynamicMotionSystem.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/ship/ShipController.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/ship/physics/ShipImpulseSystem.cpp"
)

target_include_directories(
    local_flight_control_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        local_flight_control_contract_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        local_flight_control_contract_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME local_flight_control_contracts
    COMMAND local_flight_control_contract_tests
)

set_tests_properties(
    local_flight_control_contracts
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)


add_executable(
    headless_server_boundary_compile_tests
    HeadlessServerBoundaryCompileTests.cpp
)

target_include_directories(
    headless_server_boundary_compile_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

# The main game target enables GLM's GTX extensions globally. This standalone
# compile-contract intentionally omits render/OpenGL dependencies, but must still
# compile the shared CPU geometry under the same GLM language contract.
target_compile_definitions(
    headless_server_boundary_compile_tests
    PRIVATE
        GLM_ENABLE_EXPERIMENTAL
)

if(MSVC)
    target_compile_options(
        headless_server_boundary_compile_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        headless_server_boundary_compile_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME headless_server_boundary_compile
    COMMAND headless_server_boundary_compile_tests
)

set_tests_properties(
    headless_server_boundary_compile
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    debug_session_boundary_contract_tests
    DebugSessionBoundaryContractTests.cpp
    "${ELITE_SOURCE_ROOT}/src/game/debug/LocalDebugSessionControl.cpp"
)

target_include_directories(
    debug_session_boundary_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_compile_definitions(
    debug_session_boundary_contract_tests
    PRIVATE
        GLM_ENABLE_EXPERIMENTAL
)

if(MSVC)
    target_compile_options(
        debug_session_boundary_contract_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        debug_session_boundary_contract_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME debug_session_boundary_contracts
    COMMAND debug_session_boundary_contract_tests
)

set_tests_properties(
    debug_session_boundary_contracts
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

find_package(Threads REQUIRED)

add_executable(
    thread_boundary_channel_contract_tests
    ThreadBoundaryChannelContractTests.cpp
    "${ELITE_SOURCE_ROOT}/src/game/network/LocalLoopbackTransport.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/debug/LocalDebugSessionControl.cpp"
)

target_include_directories(
    thread_boundary_channel_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_compile_definitions(
    thread_boundary_channel_contract_tests
    PRIVATE
        GLM_ENABLE_EXPERIMENTAL
)

target_link_libraries(
    thread_boundary_channel_contract_tests
    PRIVATE
        Threads::Threads
)

if(MSVC)
    target_compile_options(
        thread_boundary_channel_contract_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        thread_boundary_channel_contract_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME thread_boundary_channels
    COMMAND thread_boundary_channel_contract_tests
)

set_tests_properties(
    thread_boundary_channels
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    module_replication_contract_tests
    ModuleReplicationContractTests.cpp
)

target_include_directories(
    module_replication_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        module_replication_contract_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        module_replication_contract_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME module_replication_contracts
    COMMAND module_replication_contract_tests
)

set_tests_properties(
    module_replication_contracts
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)


add_executable(
    system_map_celestial_migration_contract_tests
    SystemMapCelestialMigrationContractTests.cpp
)

target_include_directories(
    system_map_celestial_migration_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        system_map_celestial_migration_contract_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        system_map_celestial_migration_contract_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME system_map_celestial_migration_contracts
    COMMAND system_map_celestial_migration_contract_tests
)

set_tests_properties(
    system_map_celestial_migration_contracts
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    system_map_ship_sampling_contract_tests
    SystemMapShipSamplingContractTests.cpp
)

target_include_directories(
    system_map_ship_sampling_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        system_map_ship_sampling_contract_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        system_map_ship_sampling_contract_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME system_map_ship_sampling_contracts
    COMMAND system_map_ship_sampling_contract_tests
)

set_tests_properties(
    system_map_ship_sampling_contracts
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    galaxy_map_catalog_migration_contract_tests
    GalaxyMapCatalogMigrationContractTests.cpp
)

target_include_directories(
    galaxy_map_catalog_migration_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        galaxy_map_catalog_migration_contract_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        galaxy_map_catalog_migration_contract_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME galaxy_map_catalog_migration_contracts
    COMMAND galaxy_map_catalog_migration_contract_tests
)

set_tests_properties(
    galaxy_map_catalog_migration_contracts
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

