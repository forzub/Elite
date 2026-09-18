#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/damage/HitComponent.h"
#include "src/world/navigation/NavigationObstacle.h"

namespace game::navigation
{

// Converts the authoritative damage/collision hit-volume product into
// navigation geometry. The adapter never scans render meshes and never invents
// a second object shape: active non-support HitVolume OBBs are the source.
//
// HitVolume centres/bases are object-local. objectWorldPositionMeters and
// objectLocalToWorld therefore provide the owning entity transform.
class NavigationHitVolumeAdapter final
{
public:
    struct Options
    {
        bool includeSupportLinkVolumes = false;
        double requiredClearanceMeters = 0.0;
    };

    [[nodiscard]] static std::vector<world::navigation::NavigationObstacle>
    buildObstacles(
        const game::damage::HitComponent& hitComponent,
        std::uint32_t entityId,
        const glm::dvec3& objectWorldPositionMeters,
        const glm::dmat3& objectLocalToWorld,
        const std::string& idPrefix,
        const Options& options = {}
    );

    // Conservative sphere around the entity origin containing all included
    // local HitVolume OBBs. Useful for NavigationMap broadphase actors while
    // exact per-volume OBBs stay available for precision/static geometry.
    [[nodiscard]] static double conservativeRadiusFromOrigin(
        const game::damage::HitComponent& hitComponent,
        const Options& options = {}
    ) noexcept;
};

} // namespace game::navigation
