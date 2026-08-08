#pragma once

#include <glm/glm.hpp>

#include "src/game/navigation/HubFrameBasis.h"
#include "src/game/simulation/HubAttachmentSnapshot.h"
#include "src/game/simulation/ShipReferenceFrameSnapshot.h"

namespace game::client
{


inline bool canResolveHubLocalPosition(
    int systemId,
    const std::string& hubId,
    const game::simulation::ShipReferenceFrameSnapshot& frame
) noexcept
{
    return
        systemId >= 0 &&
        frame.valid &&
        systemId == frame.systemId &&
        !hubId.empty() &&
        hubId == frame.hubId;
}

inline glm::dvec3 resolveHubLocalPosition(
    const glm::dvec3& localPositionMeters,
    const game::simulation::ShipReferenceFrameSnapshot& frame
)
{
    return frame.localToWorldPosition(localPositionMeters);
}

struct HubAttachedPresentationPose
{
    glm::dvec3 worldPositionMeters {0.0};
    glm::mat4 worldOrientation {1.0f};
    bool valid = false;
};

inline bool canResolveHubAttachedPresentation(
    const game::simulation::HubAttachmentSnapshot& attachment,
    const game::simulation::ShipReferenceFrameSnapshot& frame
) noexcept
{
    return
        attachment.valid &&
        attachment.inheritHubOrientation &&
        frame.valid &&
        attachment.systemId >= 0 &&
        attachment.systemId == frame.systemId &&
        !attachment.hubId.empty() &&
        attachment.hubId == frame.hubId;
}

inline HubAttachedPresentationPose resolveHubAttachedObjectPresentation(
    const game::simulation::HubAttachmentSnapshot& attachment,
    const game::simulation::ShipReferenceFrameSnapshot& frame
)
{
    HubAttachedPresentationPose pose;

    if (!canResolveHubAttachedPresentation(attachment, frame))
        return pose;

    pose.worldPositionMeters =
        game::navigation::hubVisualLocalToWorldPosition(
            frame.originMeters,
            frame.progradeAxis,
            frame.radialAxis,
            frame.normalAxis,
            attachment.localOffsetMeters
        );

    pose.worldOrientation =
        game::navigation::hubAttachedVisualOrientation(
            frame.progradeAxis,
            frame.radialAxis,
            frame.normalAxis,
            attachment.localRotationDeg
        );
    pose.valid = true;
    return pose;
}

} // namespace game::client
