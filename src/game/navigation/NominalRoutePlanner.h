#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/navigation/GeometricPathPlanner.h"

namespace game::navigation
{

// Stage-1 navigation product.
//
// This is deliberately a NOMINAL STATIC route.  It answers only:
// "through which geometric polyline can the vehicle generally reach the goal
// through the current static world?"
//
// Moving actors are not baked into this product.  They belong to the later
// local/dynamic overlay.  A dynamic-world revision therefore never invalidates
// a nominal route by itself.
class NominalRoutePlanner
{
public:
    struct Request
    {
        std::uint64_t goalRevision = 0;
        std::uint64_t staticWorldRevision = 0;

        glm::dvec3 startMapMeters {0.0};
        glm::dvec3 goalMapMeters {0.0};

        // Optional semantic/user checkpoints.  The planner must pass them in
        // the authored order while finding static detours for every leg.
        std::vector<glm::dvec3> requiredWaypointsMapMeters;

        std::vector<world::navigation::NavigationObstacle> staticObstacles;

        // Coarse route/corridor envelope only.  This is NOT the exact
        // time-parameterized swept-hull/tunnel proof.
        double navigationEnvelopeRadiusMeters = 0.0;
        double additionalRouteClearanceMeters = 0.0;

        // Geometric search policy crosses the Stage-1 API explicitly. The
        // planner may override only radius/clearance above because those are
        // vehicle/scenario data, not search doctrine.
        world::navigation::GeometricPathPlannerParams geometricPolicy {};
    };

    struct Plan
    {
        bool valid = false;
        bool staticDetourUsed = false;

        std::uint64_t goalRevision = 0;
        std::uint64_t staticWorldRevision = 0;

        double lengthMeters = 0.0;
        std::vector<glm::dvec3> pointsMapMeters;
        std::string message;
    };

    enum class InvalidationReason
    {
        None = 0,
        InvalidPlan,
        GoalChanged,
        StaticWorldChanged
    };

    struct ValidityQuery
    {
        std::uint64_t goalRevision = 0;
        std::uint64_t staticWorldRevision = 0;

        // Present intentionally: dynamic changes are observed by the runtime
        // but do not invalidate/rebuild the nominal static route.
        std::uint64_t dynamicWorldRevision = 0;
    };

    [[nodiscard]] static Plan plan(const Request& request);

    [[nodiscard]] static InvalidationReason invalidationReason(
        const Plan& plan,
        const ValidityQuery& current
    ) noexcept;
};

} // namespace game::navigation
