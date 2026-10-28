#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/traffic/RouteStageCompiler.h"

namespace game::navigation::traffic
{

// Portal as a window (Step 1b).
//
// A mandatory straight volume transit (portal -> straight tube -> portal,
// with its protected boundary straights) does not have to be flown through
// the geometric centre of its portals: any crossing inside the window that
// keeps the ship (agent radius) plus the volume's required clearance inside
// the volume is admissible. For each such block this chooses one lateral
// offset in the portal plane, applied to the WHOLE block (portals, authored
// centreline, boundary straights), so the block stays axis-parallel and
// inside the eroded volume, and the free legs before and after it need less
// turning. Everything downstream (composer, containment validator, stage
// adapter) sees the shifted points; the volume itself is not moved.
//
// Bent tunnels (sections with different axes) keep their centreline.
struct WindowCrossingChoice
{
    std::size_t stageIndex = 0;
    glm::dvec3 offsetWorldMeters {0.0};
    double admissibleRadiusMeters = 0.0;
    double turnBeforeRad = 0.0;
    double turnAfterRad = 0.0;
    double centredTurnRad = 0.0;
};

namespace window_crossing_detail
{
[[nodiscard]] inline double angleBetween(
    const glm::dvec3& a, const glm::dvec3& b) noexcept
{
    const double la = glm::length(a);
    const double lb = glm::length(b);
    if (!(la > 1.0e-9 && lb > 1.0e-9))
        return 0.0;
    return std::acos(std::clamp(glm::dot(a / la, b / lb), -1.0, 1.0));
}

[[nodiscard]] inline bool near(
    const glm::dvec3& a, const glm::dvec3& b) noexcept
{
    return glm::length(a - b) <= 1.0e-6;
}
} // namespace window_crossing_detail

// `startMeters` / `goalMeters`: where the route begins and ends.
// Returns the choices made (empty when nothing could be moved).
inline std::vector<WindowCrossingChoice> chooseWindowCrossings(
    CompiledTrafficRoute& route,
    const glm::dvec3& startMeters,
    const glm::dvec3& goalMeters,
    double agentRadiusMeters
)
{
    using namespace window_crossing_detail;
    std::vector<WindowCrossingChoice> choices;
    if (!route.valid)
        return choices;

    // Use at most this share of the admissible window: the block's own
    // straights are protected, and the free legs still need room to round
    // their joins inside the volume margin.
    constexpr double WindowShare = 0.9;
    // Candidate offsets: rings at these fractions of the admissible radius.
    constexpr int Rings = 4;
    constexpr int Spokes = 16;
    // Clearance is worth keeping: moving to the window's edge must buy at
    // least this much less turning (quadratic in the used share), so a
    // negligible gain on long legs leaves the crossing centred.
    constexpr double EdgeCostRad = 2.0 * 3.14159265358979323846 / 180.0;

    const auto boundaryFor =
        [&](const std::string& portalId, bool inbound)
            -> CompiledMandatoryTangentStraight*
        {
            for (auto& straight : route.mandatoryTangentStraights)
                if (straight.inbound == inbound &&
                    straight.portalId == portalId)
                    return &straight;
            return nullptr;
        };

    glm::dvec3 previous = startMeters;
    for (std::size_t index = 0; index < route.stages.size(); ++index)
    {
        auto& stage = route.stages[index];
        const bool mandatory =
            stage.kind == TrafficRouteStageKind::VolumeTransit &&
            (stage.volumeConstraint.policy ==
                 world::navigation::NavigationVolumePolicy::KeepInside ||
             stage.volumeConstraint.policy ==
                 world::navigation::NavigationVolumePolicy::PreferInside);
        if (!mandatory || stage.volumeSections.empty())
        {
            if (mandatory)
                previous = stage.toWorldMeters;
            continue;
        }

        auto* inbound = boundaryFor(stage.fromPortalId, true);
        auto* outbound = boundaryFor(stage.toPortalId, false);
        const glm::dvec3 blockStart =
            inbound ? inbound->startWorldMeters : stage.fromWorldMeters;
        const glm::dvec3 blockEnd =
            outbound ? outbound->endWorldMeters : stage.toWorldMeters;

        // Straight block only: every section shares one axis.
        const glm::dvec3 axis =
            glm::normalize(stage.volumeSections.front().forwardWorld);
        bool straight = true;
        double admissible = std::numeric_limits<double>::infinity();
        const double inset =
            std::max(0.0, agentRadiusMeters) +
            std::max(0.0, stage.volumeRequiredClearanceMeters);
        for (const auto& section : stage.volumeSections)
        {
            straight = straight &&
                angleBetween(section.forwardWorld, axis) <= 1.0e-3;
            const double half =
                section.crossSection ==
                        world::navigation::NavigationCrossSectionKind::Circle
                    ? section.radiusMeters
                    : std::min(section.halfWidthMeters,
                               section.halfHeightMeters);
            admissible = std::min(admissible, half - inset);
        }
        // The next mandatory block's start (or the goal) is where the free
        // leg after this block heads.
        glm::dvec3 next = goalMeters;
        for (std::size_t later = index + 1; later < route.stages.size();
             ++later)
        {
            const auto& candidate = route.stages[later];
            if (candidate.kind == TrafficRouteStageKind::VolumeTransit &&
                !candidate.volumeSections.empty())
            {
                const auto* in = boundaryFor(candidate.fromPortalId, true);
                next = in ? in->startWorldMeters : candidate.fromWorldMeters;
                break;
            }
        }
        if (!straight || !(admissible > 1.0e-3))
        {
            previous = blockEnd;
            continue;
        }
        admissible *= WindowShare;

        glm::dvec3 right = glm::cross(axis, stage.volumeSections.front().upWorld);
        if (glm::length(right) <= 1.0e-9)
            right = glm::cross(axis, glm::dvec3(0.0, 1.0, 0.0));
        right = glm::normalize(right);
        const glm::dvec3 up = glm::normalize(glm::cross(right, axis));

        const auto turnCost = [&](const glm::dvec3& offset)
        {
            return angleBetween(blockStart + offset - previous, axis) +
                angleBetween(next - (blockEnd + offset), axis);
        };
        const double centred = turnCost(glm::dvec3(0.0));
        glm::dvec3 best {0.0};
        double bestCost = centred;
        for (int ring = 1; ring <= Rings; ++ring)
        {
            const double radius =
                admissible * static_cast<double>(ring) / Rings;
            for (int spoke = 0; spoke < Spokes; ++spoke)
            {
                const double a =
                    2.0 * 3.14159265358979323846 * spoke / Spokes;
                const glm::dvec3 offset =
                    (right * std::cos(a) + up * std::sin(a)) * radius;
                const double share = radius / admissible;
                const double cost =
                    turnCost(offset) + EdgeCostRad * share * share;
                if (cost < bestCost)
                {
                    bestCost = cost;
                    best = offset;
                }
            }
        }

        if (glm::length(best) > 1.0e-6)
        {
            // Shift every copy of the block's points: the stage itself, the
            // free stages that end/begin at its portals, its straights and
            // the via-point list.
            const std::vector<glm::dvec3> moved = [&]()
            {
                std::vector<glm::dvec3> points {
                    stage.fromWorldMeters, stage.toWorldMeters};
                for (const auto& p : stage.requiredCenterlineMeters)
                    points.push_back(p);
                if (inbound)
                    points.push_back(inbound->startWorldMeters);
                if (outbound)
                    points.push_back(outbound->endWorldMeters);
                return points;
            }();
            const auto shift = [&](glm::dvec3& point)
            {
                for (const auto& m : moved)
                    if (near(point, m))
                    {
                        point += best;
                        return;
                    }
            };
            for (auto& other : route.stages)
            {
                shift(other.fromWorldMeters);
                shift(other.toWorldMeters);
                for (auto& p : other.requiredCenterlineMeters)
                    shift(p);
            }
            for (auto& s : route.mandatoryTangentStraights)
            {
                shift(s.startWorldMeters);
                shift(s.endWorldMeters);
            }
            for (auto& p : route.requiredViaPointsMeters)
                shift(p);
        }

        WindowCrossingChoice choice;
        choice.stageIndex = index;
        choice.offsetWorldMeters = best;
        choice.admissibleRadiusMeters = admissible;
        choice.centredTurnRad = centred;
        choice.turnBeforeRad =
            angleBetween(blockStart + best - previous, axis);
        choice.turnAfterRad = angleBetween(next - (blockEnd + best), axis);
        choices.push_back(choice);
        previous = blockEnd + best;
    }
    return choices;
}

} // namespace game::navigation::traffic
