#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/planner/RoutePlannerApi.h"

namespace game::navigation
{

// Dock-owned frame field for the visible/executable docking corridor.
//
// Planner owns only centerline geometry and tangent. The selected dock owns the
// terminal roll phase (which side of the entrance is "up"). This helper
// transports that terminal orientation backwards through the authored route so
// every frame is continuous and the final frame is phase-locked to the live
// docking aperture.
class DockingCorridorFrameField
{
public:
    [[nodiscard]] static std::vector<glm::dvec3> buildUpVectors(
        const std::vector<planner::RouteGate>& gates,
        const glm::dvec3& terminalUpRequested
    )
    {
        std::vector<glm::dvec3> out(
            gates.size(),
            glm::dvec3(0.0, 1.0, 0.0)
        );
        if (gates.empty())
            return out;

        const auto normalizedForward =
            [](const glm::dvec3& requested)
            {
                const double length = glm::length(requested);
                return length > 1.0e-9
                    ? requested / length
                    : glm::dvec3(0.0, 0.0, -1.0);
            };

        const auto projectedUp =
            [](const glm::dvec3& requested,
               const glm::dvec3& forward,
               const glm::dvec3& continuityReference)
            {
                glm::dvec3 up =
                    requested -
                    forward * glm::dot(requested, forward);

                if (glm::length(up) <= 1.0e-9)
                {
                    const glm::dvec3 seed =
                        std::abs(forward.y) < 0.90
                            ? glm::dvec3(0.0, 1.0, 0.0)
                            : glm::dvec3(1.0, 0.0, 0.0);
                    up =
                        seed -
                        forward * glm::dot(seed, forward);
                }

                up = glm::normalize(up);
                if (glm::length(continuityReference) > 1.0e-9 &&
                    glm::dot(up, continuityReference) < 0.0)
                {
                    up = -up;
                }
                return up;
            };

        const std::size_t last = gates.size() - 1;
        const glm::dvec3 lastForward =
            normalizedForward(gates[last].forward);

        glm::dvec3 terminalUp = terminalUpRequested;
        if (glm::length(terminalUp) <= 1.0e-9)
            terminalUp = glm::dvec3(0.0, 1.0, 0.0);

        out[last] =
            projectedUp(
                terminalUp,
                lastForward,
                terminalUp
            );

        for (std::size_t index = last; index > 0; --index)
        {
            const glm::dvec3 previousForward =
                normalizedForward(gates[index - 1].forward);
            out[index - 1] =
                projectedUp(
                    out[index],
                    previousForward,
                    out[index]
                );
        }

        return out;
    }
};

} // namespace game::navigation
