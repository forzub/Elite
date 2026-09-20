#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace elite::tools::navigation_runtime
{

struct TraceFrame
{
    double timeSeconds = 0.0;

    glm::dvec3 shipPosition {0.0};
    glm::dvec3 shipForward {1.0, 0.0, 0.0};
    glm::dvec3 shipRight {0.0, 0.0, 1.0};
    glm::dvec3 shipUp {0.0, 1.0, 0.0};
    glm::dvec3 shipVelocity {0.0};

    // Reference state sampled from the actually accepted maneuver program.
    // This is distinct from actual rigid-body attitude and lets the viewer
    // expose tracking/orientation mismatch instead of reconstructing attitude.
    bool hasProgramReference = false;
    glm::dvec3 programReferencePosition {0.0};
    glm::dvec3 programReferenceForward {1.0, 0.0, 0.0};
    glm::dvec3 programReferenceRight {0.0, 0.0, 1.0};
    glm::dvec3 programReferenceUp {0.0, 1.0, 0.0};
    double programTrackingCorridorRadiusMeters = 0.0;

    bool hazardActive = false;
    glm::dvec3 hazardPosition {0.0};
    glm::dvec3 hazardVelocity {0.0};
    double plannerLookAheadSeconds = 0.0;
    double hazardRadiusMeters = 0.0;
    double hazardCollisionEnvelopeRadiusMeters = 0.0;
    double hazardPlannerEnvelopeRadiusMeters = 0.0;
    double dynamicClearanceMeters = 0.0;

    std::string phase;
    std::string plannerStatus;
    bool replanEvent = false;

    bool hasSelectedTarget = false;
    glm::dvec3 selectedTarget {0.0};

    bool hasReacquisitionTarget = false;
    glm::dvec3 reacquisitionTarget {0.0};

    bool hasPortalTarget = false;
    glm::dvec3 portalTarget {0.0};
};

struct TraceStaticObstacle
{
    std::string id;
    std::string shape = "sphere";
    glm::dvec3 center {0.0};
    glm::dvec3 halfExtents {1.0};
    double radiusMeters = 1.0;
    double capsuleHalfLengthMeters = 0.0;
};

struct TraceDocument
{
    int version = 2;
    std::string law;
    glm::dvec3 shipHalfExtentsMeters {1.0};

    // Authored scene endpoints exist before any planner result. The viewer
    // must be able to display/focus the scene before Calculate is pressed.
    bool hasSceneEndpoints = false;
    glm::dvec3 sceneStartMapMeters {0.0};
    glm::dvec3 sceneFinishMapMeters {0.0};

    std::vector<glm::dvec3> routePoints;
    std::vector<glm::dvec3> turnPoints;
    std::vector<TraceStaticObstacle> staticObstacles;
    std::vector<TraceFrame> frames;
};

void saveTraceJson(
    const TraceDocument& trace,
    const std::string& path
);

TraceDocument loadTraceJson(const std::string& path);

} // namespace elite::tools::navigation_runtime
