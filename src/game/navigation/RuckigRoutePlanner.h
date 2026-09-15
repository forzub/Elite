#pragma once

#include "src/world/navigation/TrajectoryGenerator.h"

namespace game::navigation
{

/*
    Canonical runtime route-to-trajectory backend.

    Responsibilities are deliberately narrow:
      * consume an already collision-free coarse polyline;
      * solve each local state-to-state leg with Ruckig;
      * validate the resulting swept leg against canonical navigation geometry;
      * return one immutable, time-parameterized trajectory product.

    It does NOT search the obstacle topology and it does NOT render guidance.
    GeometricPathPlanner owns coarse obstacle bypass; GuidanceTunnel is only a
    presentation sampler over this accepted trajectory. Player and NPC runtime
    navigation can therefore share this backend without pulling HUD state into
    motion planning.
*/
class RuckigRoutePlanner
{
public:
    static world::navigation::TrajectoryGenerationResult plan(
        const world::navigation::TrajectoryGenerationRequest& request
    );
};

} // namespace game::navigation
