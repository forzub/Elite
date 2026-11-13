#pragma once

// Jump currents (author 2026-11-13, JUMP_MECHANICS.md sections 2 and 4): gravity acts on the jump; the step drifts
// down the field's gradient, and harder toward a sump ("where the pull is stronger, the drift is stronger").
//
// Plausible cause (unproven, not absurd): the same law as gravitational lensing - light passing a mass is bent by
// (2/c^2) * integral of the transverse pull along its path. The jump field is a far "softer" medium than light (its
// effective speed ~ hundreds of km/s, not c), and between the micro-exits of the steps the bend becomes a sideways
// shift of the exit:
//     drift = CurrentShiftS2PerM * integral g_perp ds   (every mass on the line, its own body radius softens it)
// A uniform field shifts nothing (the system falls in it as a whole - the Galaxy's pull is out, only local masses).
// A swarm (sump) adds its own drift toward its densest ridge, SumpDriftPerDepthM per unit of depth crossed
// (depth = column / SumpColumnKgM2, JumpSumps.h): the snag of the rough potential, not the swarm's tiny mass.
// Beacons measure the currents: the planner compensates them, the residual is drift / lock (no beacon - all of it).
// Calibration: Earth -> Jupiter past the Sun ~ 10^5..10^6 km; the main belt crossing ~ 5*10^5 km toward its ridge.
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/rules/JumpRules.h"
#include "src/game/navigation/rules/JumpSumps.h"

namespace game::navigation::jump
{
constexpr double CurrentShiftS2PerM = 0.3;
constexpr double SumpDriftPerDepthM = 0.01 * AuM;

struct CurrentMass
{
    glm::dvec3 centreM {0.0};
    double muM3s2 = 0.0;
    double radiusM = 0.0;
};

// the drift of the straight step a -> b (system frame, metres): masses + the sumps' pull
inline glm::dvec3 jumpCurrentDrift(const std::vector<CurrentMass>& masses, const std::vector<JumpSump>& sumps,
                                   const std::unordered_map<std::string, glm::dvec3>& centres, const glm::dvec3& a, const glm::dvec3& b)
{
    const double length = glm::length(b - a);
    if (length <= 1.0)
        return glm::dvec3(0.0);
    const glm::dvec3 u = (b - a) / length;
    // masses: the transverse integral, analytic for a straight line past a softened point mass
    glm::dvec3 drift(0.0);
    for (const auto& m : masses)
    {
        if (m.muM3s2 <= 0.0)
            continue;
        const glm::dvec3 rel = m.centreM - a;
        const double along = glm::dot(rel, u);
        glm::dvec3 perp = rel - u * along;
        const double bImpact = std::max(glm::length(perp), std::max(1.0, m.radiusM));
        if (glm::length(perp) < 1.0e-6)
            continue;   // straight at it: no transverse pull (the line never passes a body - the planner's rule)
        perp = glm::normalize(perp);
        // integral of mu * b / (b^2 + s^2)^(3/2) ds from s = -along to s = length - along
        const double s0 = -along, s1 = length - along;
        const double I = m.muM3s2 / bImpact * (s1 / std::sqrt(bImpact * bImpact + s1 * s1) - s0 / std::sqrt(bImpact * bImpact + s0 * s0));
        drift += perp * (CurrentShiftS2PerM * I);
    }
    // sumps: toward the swarm's ridge (peak radius in its plane), weighted by the depth crossed
    constexpr int N = 200;
    const double dl = length / N;
    for (const auto& s : sumps)
    {
        const auto c = centres.find(s.centreBodyId);
        if (c == centres.end())
            continue;
        for (int i = 0; i < N; ++i)
        {
            const glm::dvec3 p = a + u * ((i + 0.5) * dl);
            const double rho = sumpDensityKgM3(s, p, centres);
            if (rho <= 0.0)
                continue;
            glm::dvec3 local = p - c->second;
            glm::dvec3 inPlane(local.x, 0.0, local.z);
            if (glm::length(inPlane) < 1.0)
                continue;
            const glm::dvec3 ridge = c->second + glm::normalize(inPlane) * s.peakM;
            glm::dvec3 toward = ridge - p;
            toward -= u * glm::dot(toward, u);
            if (glm::length(toward) < 1.0)
                continue;
            drift += glm::normalize(toward) * (SumpDriftPerDepthM * rho * dl / SumpColumnKgM2);
        }
    }
    return drift;
}
} // namespace game::navigation::jump
