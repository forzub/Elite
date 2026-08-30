cmake_minimum_required(VERSION 3.20)
project(ModelAssetTests LANGUAGES CXX)
include(CTest)
include(FetchContent)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
get_filename_component(ELITE_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
set(LIBIGL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LIBIGL_BUILD_TUTORIALS OFF CACHE BOOL "" FORCE)
set(LIBIGL_INSTALL OFF CACHE BOOL "" FORCE)
set(LIBIGL_USE_STATIC_LIBRARY OFF CACHE BOOL "" FORCE)
set(LIBIGL_EMBREE ON CACHE BOOL "" FORCE)
set(LIBIGL_OPENGL OFF CACHE BOOL "" FORCE)
set(LIBIGL_GLFW OFF CACHE BOOL "" FORCE)
set(LIBIGL_IMGUI OFF CACHE BOOL "" FORCE)
set(LIBIGL_PREDICATES OFF CACHE BOOL "" FORCE)
set(LIBIGL_XML OFF CACHE BOOL "" FORCE)
set(LIBIGL_COPYLEFT_CORE OFF CACHE BOOL "" FORCE)
set(LIBIGL_COPYLEFT_CGAL OFF CACHE BOOL "" FORCE)
set(LIBIGL_COPYLEFT_COMISO OFF CACHE BOOL "" FORCE)
set(LIBIGL_COPYLEFT_TETGEN OFF CACHE BOOL "" FORCE)
set(LIBIGL_RESTRICTED_MATLAB OFF CACHE BOOL "" FORCE)
set(LIBIGL_RESTRICTED_MOSEK OFF CACHE BOOL "" FORCE)
set(LIBIGL_RESTRICTED_TRIANGLE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(libigl GIT_REPOSITORY https://github.com/libigl/libigl.git GIT_TAG v2.6.0 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(libigl)
add_executable(model_asset_tests
    ModelAssetBinaryTests.cpp
    "${ELITE_SOURCE_ROOT}/src/model_asset/ModelAssetBinary.cpp"
    "${ELITE_SOURCE_ROOT}/src/model_asset/ModelAssetMigration.cpp"
    "${ELITE_SOURCE_ROOT}/src/model_asset/RuntimeMeshNormalizer.cpp"
    "${ELITE_SOURCE_ROOT}/tools/model_asset_editor/NativeObjImporter.cpp"
    "${ELITE_SOURCE_ROOT}/tools/model_asset_editor/CanonicalMeshBuilder.cpp"
    "${ELITE_SOURCE_ROOT}/tools/model_asset_editor/GeometryInstanceFitter.cpp"
)
target_include_directories(model_asset_tests PRIVATE "${ELITE_SOURCE_ROOT}" "${ELITE_SOURCE_ROOT}/src")
target_link_libraries(model_asset_tests PRIVATE igl::core igl::embree)
if(MSVC)
    target_compile_options(model_asset_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(model_asset_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME model_asset COMMAND model_asset_tests)
