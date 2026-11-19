cmake_minimum_required(VERSION 3.20)

project(RefuelRulesTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(ELITE_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

find_package(nlohmann_json REQUIRED)

add_executable(
    refuel_rules_tests
    RefuelRulesTests.cpp
    "${ELITE_SOURCE_ROOT}/src/game/fuel/FuelStationCatalog.cpp"
)
target_include_directories(refuel_rules_tests PRIVATE "${ELITE_SOURCE_ROOT}")
target_link_libraries(refuel_rules_tests PRIVATE nlohmann_json::nlohmann_json)
target_compile_definitions(refuel_rules_tests PRIVATE ELITE_SOURCE_ROOT="${ELITE_SOURCE_ROOT}")
