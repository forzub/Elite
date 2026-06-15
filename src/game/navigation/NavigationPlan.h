#pragma once

#include <string>
#include <glm/glm.hpp>

namespace game::navigation
{

enum class NavigationPlanType
{
    None,
    CruiseRoute,
    JumpRoute
};

enum class NavigationPlanState
{
    Empty,
    Planned,
    Active,
    Completed,
    Failed
};

struct NavigationPlan
{
    NavigationPlanType type =
        NavigationPlanType::None;

    NavigationPlanState state =
        NavigationPlanState::Empty;

    std::string targetSystemId;
    std::string targetBodyId;
    std::string targetHubId;

    glm::dvec3 plannedExitPositionMeters {0.0};
    glm::dvec3 plannedExitVelocityMps {0.0};

    double plannedExitTimeSeconds = 0.0;
    double arrivalErrorMeters = 0.0;
    double arrivalAngleErrorDeg = 0.0;

    bool valid = false;
};

} // namespace game::navigation