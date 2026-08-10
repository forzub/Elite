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
