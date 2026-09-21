#pragma once

#include "src/world/navigation/TrajectoryGenerator.h"

namespace game::navigation
{

/*
    Canonical runtime route-to-trajectory backend.

    Responsibilities are deliberately narrow:
      * consume an already collision-free coarse polyline;
      * derive a local continuous execution guide without changing topology;
      * keep spatial geometry p(s) separate from timing s(t);
      * use Ruckig state-to-state for a true single leg and scalar Ruckig
        progress for curved/multi-point routes;
      * validate the resulting swept trajectory against canonical navigation
        geometry;
      * return one immutable, time-parameterized trajectory product.

    Dense geometric samples are NOT fed to Ruckig as independent 3-D target
    states. Ruckig's own documentation recommends few, well-separated
    intermediate waypoints; our open-source integration does not use the Pro
    intermediate-waypoint solver.

    It does NOT search the obstacle topology and it does NOT render guidance.
    GeometricPathPlanner owns coarse obstacle bypass; GuidanceTunnel is only a
    presentation sampler over this accepted trajectory.
*/
class RuckigRoutePlanner
{
public:
    static world::navigation::TrajectoryGenerationResult plan(
        const world::navigation::TrajectoryGenerationRequest& request
    );
};

} // namespace game::navigation
