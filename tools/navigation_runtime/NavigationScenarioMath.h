#pragma once

#include <cmath>

#include "NavigationScenarioRuntime.h"

namespace elite::tools::navigation_runtime
{

// Shared pure scenario-space math used by both authored-input adaptation and
// runtime composition. Keeping these helpers outside the I/O module prevents
// file/parser ownership changes from hiding calculation dependencies.
inline glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
)
{
    const double length = glm::length(value);
    if (!(length > 1.0e-9))
        return fallback;
    return value / length;
}

inline ScenarioBasis basisFromForwardUp(
    const glm::dvec3& forwardInput,
    const glm::dvec3& upInput
)
{
    const glm::dvec3 forward =
        normalizedOr(forwardInput, {1.0, 0.0, 0.0});

    glm::dvec3 up =
        upInput - forward * glm::dot(upInput, forward);

    if (glm::length(up) <= 1.0e-9)
    {
        up = {0.0, 1.0, 0.0};
        if (std::abs(glm::dot(up, forward)) > 0.92)
            up = {0.0, 0.0, 1.0};
        up -= forward * glm::dot(up, forward);
    }

    up = glm::normalize(up);
    const glm::dvec3 right =
        glm::normalize(glm::cross(forward, up));
    up = glm::normalize(glm::cross(right, forward));
    return {forward, right, up};
}

} // namespace elite::tools::navigation_runtime
