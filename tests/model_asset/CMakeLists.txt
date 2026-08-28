cmake_minimum_required(VERSION 3.20)
project(ModelAssetTests LANGUAGES CXX)
include(CTest)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
get_filename_component(ELITE_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
add_executable(model_asset_tests
    ModelAssetBinaryTests.cpp
    "${ELITE_SOURCE_ROOT}/src/model_asset/ModelAssetBinary.cpp"
    "${ELITE_SOURCE_ROOT}/src/model_asset/ModelAssetMigration.cpp"
    "${ELITE_SOURCE_ROOT}/tools/model_asset_editor/NativeObjImporter.cpp"
    "${ELITE_SOURCE_ROOT}/tools/model_asset_editor/CanonicalMeshBuilder.cpp"
    "${ELITE_SOURCE_ROOT}/tools/model_asset_editor/GeometryInstanceFitter.cpp"
)
target_include_directories(model_asset_tests PRIVATE "${ELITE_SOURCE_ROOT}" "${ELITE_SOURCE_ROOT}/src")
if(MSVC)
    target_compile_options(model_asset_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(model_asset_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME model_asset COMMAND model_asset_tests)
