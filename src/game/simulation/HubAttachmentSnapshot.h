#pragma once

#include <string>
#include <glm/glm.hpp>

namespace game::simulation
{

/*
    Stable binding of a runtime object to the visual/model coordinate basis of
    an orbital hub.

    This is deliberately not a sampled world pose. The local offset/rotation
    remain invariant while the hub moves, which lets presentation reconstruct
    every co-frame object from one common hub-frame sample instead of mixing
    independently sampled world transforms.

    Hub visual/model basis convention:
        local X -> hub normal
        local Y -> hub radial
        local Z -> -hub prograde
*/
struct HubAttachmentSnapshot
{
    int systemId = -1;
    std::string hubId;
    std::string moduleId;

    glm::dvec3 localOffsetMeters {0.0};
    // Base module rotation at universe epoch 0. Keep it stable on the wire;
    // presentation advances it from the shared hub-frame universe time.
    glm::dvec3 localRotationDeg {0.0};
    glm::dvec3 localAngularVelocityDegPerSecond {0.0};

    bool inheritHubOrientation = true;
    bool valid = false;
};

} // namespace game::simulation
