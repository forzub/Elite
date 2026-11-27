#pragma once

// A client city object -> game::navigation::HubCitySpace (the one plan <-> world owner) in an EXPLICIT epoch:
//   presentationCitySpace  - the render pose (what the 3D scene and the HUD draw this frame)
//   authoritativeCitySpace - the last authoritative pose (what the physics / the planner reason about)
// The bend comes from the object's frame module plan (HubPlanCatalog); no plan: a flat identity bend.
#include <glm/glm.hpp>

#include "src/game/client/ClientWorldState.h"
#include "src/game/navigation/HubCitySpace.h"
#include "src/game/navigation/HubPlanCatalog.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::client
{
inline game::navigation::HubCitySpace citySpaceFromPose(const ClientObjectState& object,
                                                         const world::coordinates::WorldPosition& position,
                                                         const glm::mat4& orientation,
                                                         const glm::dvec3& linearVelocityMps,
                                                         const glm::dvec3& angularVelocityWorldRadPerSecond)
{
    // ONE epoch per space (audit #6 B1): the position, the orientation AND the velocities of the same epoch
    game::navigation::HubCitySpace space;
    space.cpos = world::coordinates::fullMeters(position);
    space.R = glm::dmat3(glm::mat3(orientation));
    space.cvel = linearVelocityMps;
    space.omega = angularVelocityWorldRadPerSecond;
    if (object.hubAttachment.valid)
        if (const auto* plan = game::navigation::HubPlanCatalog::forFrameModule(object.hubAttachment.moduleId))
            space.bend = plan->ring();
    return space;
}

inline game::navigation::HubCitySpace presentationCitySpace(const ClientObjectState& object)
{
    return citySpaceFromPose(object, object.renderWorldPosition, object.renderOrientation, object.renderLinearVelocityMps,
                             object.renderAngularVelocityWorldRadPerSecond);
}

inline game::navigation::HubCitySpace authoritativeCitySpace(const ClientObjectState& object)
{
    return citySpaceFromPose(object, object.worldPosition, object.orientation, object.linearVelocityMps,
                             object.angularVelocityWorldRadPerSecond);
}
} // namespace game::client
