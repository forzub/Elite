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
    glm::dvec3 shipVelocity {0.0};

    bool hazardActive = false;
    glm::dvec3 hazardPosition {0.0};
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

struct TraceDocument
{
    int version = 1;
    std::string law;
    glm::dvec3 shipHalfExtentsMeters {1.0};

    std::vector<glm::dvec3> routePoints;
    std::vector<glm::dvec3> turnPoints;
    std::vector<TraceFrame> frames;
};

void saveTraceJson(
    const TraceDocument& trace,
    const std::string& path
);

TraceDocument loadTraceJson(const std::string& path);

} // namespace elite::tools::navigation_runtime
