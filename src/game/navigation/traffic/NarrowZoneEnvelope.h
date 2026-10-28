#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <glm/glm.hpp>

#include "src/game/navigation/traffic/RouteStageCompiler.h"
#include "src/game/navigation/traffic/RouteVolumeContainmentValidator.h"

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
    // Room from the axis to the walls minus the required clearance, to the
    // section's right / up (for the chosen roll).
    double roomRightMeters = 0.0;
    double roomUpMeters = 0.0;
};

// Constrained rotation in a narrow volume (Step F): which hull rotations the
// walls admit. The hull is aligned (nose along the axis, up along the zone
// up) at the zone centre line. A rotation about the zone's UP axis (yaw)
// sweeps the hull in the axis-right plane (radius sqrt(L^2 + W^2)), keeping
// its height; about the zone's RIGHT axis (pitch) it sweeps the axis-up plane
// (radius sqrt(L^2 + H^2)), keeping its width; a roll about the axis sweeps
// the cross-section (radius sqrt(W^2 + H^2)). The attitude tolerance widens
// the non-swept extent; the lateral position tolerance the executor holds
// (`positionToleranceMeters`) is taken off the room in the swept plane. A 180 degree flip uses
// the safe axis with the larger margin; with none the craft must not flip
// inside the volume (brake earlier, enter oriented, fly it on RCS).
struct RotationFit
{
    bool yawFlipSafe = false;
    bool pitchFlipSafe = false;
    bool rollSafe = false;
    double yawMarginMeters = 0.0;
    double pitchMarginMeters = 0.0;
    double rollMarginMeters = 0.0;
    // Zero when no flip is safe.
    glm::dvec3 flipAxisWorld {0.0};
    double flipMarginMeters = 0.0;
};

[[nodiscard]] inline RotationFit analyseRotationFit(
    const VolumeFit& fit,
    const ShipHullBox& hull,
    double attitudeToleranceRad,
    double positionToleranceMeters = 0.0
) noexcept
{
    RotationFit out;
    if (!hull.valid())
        return out;
    const double widen =
        hull.halfLengthMeters * std::sin(std::max(0.0, attitudeToleranceRad));
    // Zone right / up already follow the chosen roll (VolumeFit), so the
    // hull's width lies along the zone right and its height along the up.
    const double hw = hull.halfWidthMeters;
    const double hh = hull.halfHeightMeters;
    const double hl = hull.halfLengthMeters;
    // The position tolerance counts in the swept plane; across it the
    // aligned containment (with that same tolerance as its margin) holds.
    const double pos = std::max(0.0, positionToleranceMeters);
    const double roomR = fit.roomRightMeters;
    const double roomU = fit.roomUpMeters;
    out.yawMarginMeters =
        std::min(roomR - pos - std::hypot(hl, hw), roomU - (hh + widen));
    out.pitchMarginMeters =
        std::min(roomU - pos - std::hypot(hl, hh), roomR - (hw + widen));
    out.rollMarginMeters = std::min(roomR, roomU) - pos - std::hypot(hw, hh);
    out.yawFlipSafe = out.yawMarginMeters >= 0.0;
    out.pitchFlipSafe = out.pitchMarginMeters >= 0.0;
    out.rollSafe = out.rollMarginMeters >= 0.0;
    const glm::dvec3 right = glm::normalize(glm::cross(fit.axisWorld, fit.upWorld));
    if (out.yawFlipSafe &&
        (!out.pitchFlipSafe || out.yawMarginMeters >= out.pitchMarginMeters))
    {
        out.flipAxisWorld = fit.upWorld;
        out.flipMarginMeters = out.yawMarginMeters;
    }
    else if (out.pitchFlipSafe)
    {
        out.flipAxisWorld = right;
        out.flipMarginMeters = out.pitchMarginMeters;
    }
    return out;
}

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
    out.roomRightMeters = roomW;
    out.roomUpMeters = roomH;
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
    {
        out.upWorld = glm::normalize(glm::cross(out.upWorld, out.axisWorld));
        std::swap(out.roomRightMeters, out.roomUpMeters);
    }
    return out;
}

// The one KeepInside check for a ship (composer and orchestration use the
// same): the bounding sphere where it fits, the aligned hull box in a
// narrow volume, a failure when neither fits.
[[nodiscard]] inline RouteVolumeContainmentResult validateKeepInsideForShip(
    const CompiledTrafficStage& stage,
    const std::vector<planner::RouteCurveSegment>& curves,
    const ShipHullBox& hull,
    double sphereRadiusMeters,
    double attitudeToleranceRad,
    VolumeFit* fitOut = nullptr
)
{
    const VolumeFit fit = analyseVolumeFit(
        stage, hull, sphereRadiusMeters, attitudeToleranceRad);
    if (fitOut)
        *fitOut = fit;
    if (!fit.sphereFits && !fit.alignedFits)
    {
        RouteVolumeContainmentResult out;
        out.failure = "the ship does not fit the mandatory volume in any attitude";
        return out;
    }
    return fit.sphereFits
        ? RouteVolumeContainmentValidator::validateKeepInsideRouteInterval(
              stage, curves, stage.fromWorldMeters, stage.toWorldMeters,
              sphereRadiusMeters)
        : RouteVolumeContainmentValidator::validateKeepInsideRouteIntervalInsets(
              stage, curves, stage.fromWorldMeters, stage.toWorldMeters,
              fit.insetRightMeters, fit.insetUpMeters);
}

} // namespace game::navigation::traffic
