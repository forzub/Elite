#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/glm.hpp>

#include "src/game/navigation/traffic/RouteStageCompiler.h"

namespace game::navigation::traffic
{

// Orientation-aware fit of a ship hull in a mandatory volume (Step 3).
//
// The hull is a box (half width X, half height Y, half length Z, body
// axes). A volume the bounding sphere fits in lets the ship turn freely
// inside it. A volume only the ALIGNED box fits in (nose along the volume
// axis, up along the volume's up or its right for a rectangle) is a narrow
// zone: crossable, but in that one attitude and without turning. The
// attitude tolerance the executor holds widens the box's cross-section by
// halfLength * sin(tolerance) on both lateral axes (conservative).
struct ShipHullBox
{
    double halfWidthMeters = 0.0;
    double halfHeightMeters = 0.0;
    double halfLengthMeters = 0.0;

    [[nodiscard]] bool valid() const noexcept
    {
        return halfWidthMeters > 0.0 && halfHeightMeters > 0.0 &&
            halfLengthMeters > 0.0;
    }
};

struct VolumeFit
{
    bool sphereFits = false;
    bool alignedFits = false;
    // Rectangle: the box fits only (or better) with its width along the
    // section's up, i.e. rolled 90 degrees.
    bool rolled90 = false;
    glm::dvec3 axisWorld {0.0, 0.0, 1.0};
    glm::dvec3 upWorld {0.0, 1.0, 0.0};
    // Insets for containment (hull extent + required clearance) and the
    // remaining lateral room, to the section's right / up.
    double insetRightMeters = 0.0;
    double insetUpMeters = 0.0;
    double marginRightMeters = 0.0;
    double marginUpMeters = 0.0;
};

// Straight volumes only (one axis); `sphereRadiusMeters` is the planner's
// bounding-sphere agent radius.
[[nodiscard]] inline VolumeFit analyseVolumeFit(
    const CompiledTrafficStage& stage,
    const ShipHullBox& hull,
    double sphereRadiusMeters,
    double attitudeToleranceRad
)
{
    VolumeFit out;
    if (stage.volumeSections.empty())
        return out;
    const auto& first = stage.volumeSections.front();
    out.axisWorld = glm::normalize(first.forwardWorld);
    glm::dvec3 up = first.upWorld - out.axisWorld *
        glm::dot(first.upWorld, out.axisWorld);
    out.upWorld = glm::length(up) > 1.0e-9 ? glm::normalize(up)
                                           : glm::dvec3(0.0, 1.0, 0.0);

    const double clearance =
        std::max(0.0, stage.volumeRequiredClearanceMeters);
    bool circle = true;
    double radius = std::numeric_limits<double>::infinity();
    double halfW = std::numeric_limits<double>::infinity();
    double halfH = std::numeric_limits<double>::infinity();
    for (const auto& section : stage.volumeSections)
    {
        if (section.crossSection ==
            world::navigation::NavigationCrossSectionKind::Circle)
            radius = std::min(radius, section.radiusMeters);
        else
        {
            circle = false;
            halfW = std::min(halfW, section.halfWidthMeters);
            halfH = std::min(halfH, section.halfHeightMeters);
        }
    }
    if (circle)
        halfW = halfH = radius;
    const double roomW = halfW - clearance;
    const double roomH = halfH - clearance;
    const double sphere = std::max(0.0, sphereRadiusMeters);
    out.sphereFits = circle ? sphere <= roomW : sphere <= std::min(roomW, roomH);
    if (out.sphereFits || !hull.valid())
    {
        out.insetRightMeters = out.insetUpMeters = sphere + clearance;
        out.marginRightMeters = roomW - sphere;
        out.marginUpMeters = roomH - sphere;
        return out;
    }

    const double widen =
        hull.halfLengthMeters * std::sin(std::max(0.0, attitudeToleranceRad));
    const double hw = hull.halfWidthMeters + widen;
    const double hh = hull.halfHeightMeters + widen;
    if (circle)
    {
        const double extent = std::sqrt(hw * hw + hh * hh);
        out.alignedFits = extent <= roomW;
        out.insetRightMeters = out.insetUpMeters = extent + clearance;
        out.marginRightMeters = out.marginUpMeters = roomW - extent;
        return out;
    }
    const double marginLevel = std::min(roomW - hw, roomH - hh);
    const double marginRolled = std::min(roomW - hh, roomH - hw);
    out.rolled90 = marginRolled > marginLevel;
    const double margin = std::max(marginLevel, marginRolled);
    out.alignedFits = margin >= 0.0;
    // Insets and margins along the section's right / up for the chosen roll.
    const double extentRight = out.rolled90 ? hh : hw;
    const double extentUp = out.rolled90 ? hw : hh;
    out.insetRightMeters = extentRight + clearance;
    out.insetUpMeters = extentUp + clearance;
    out.marginRightMeters = roomW - extentRight;
    out.marginUpMeters = roomH - extentUp;
    if (out.rolled90)
        out.upWorld = glm::normalize(glm::cross(out.upWorld, out.axisWorld));
    return out;
}

} // namespace game::navigation::traffic
