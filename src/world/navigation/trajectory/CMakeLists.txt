get_filename_component(
    ELITE_NAVIGATION_TRAJECTORY_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../.."
    ABSOLUTE
)

add_library(EliteNavigationTrajectory STATIC
    OrientedPassageEvaluator.cpp
    BoundedGapCandidateBuilder.cpp
    AttitudeReachabilityEvaluator.cpp
    EmergencyPassageMitigator.cpp
    ContinuousPassageTrajectoryEvaluator.cpp
    EmergencyContactSeverityScorer.cpp
)

target_include_directories(EliteNavigationTrajectory PUBLIC
    "${ELITE_NAVIGATION_TRAJECTORY_ROOT}"
    "${ELITE_NAVIGATION_TRAJECTORY_ROOT}/src"
)

target_compile_features(EliteNavigationTrajectory PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(EliteNavigationTrajectory PRIVATE /W4 /permissive-)
else()
    target_compile_options(EliteNavigationTrajectory PRIVATE -Wall -Wextra -Wpedantic)
endif()
