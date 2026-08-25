#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "src/world/navigation/NavigationObstacle.h"
#include "src/world/navigation/NavigationVehicleProfile.h"
#include "src/world/navigation/Trajectory.h"

namespace world::navigation
{

/*
    Spatial manual-flight guide sampled at equal arc-length intervals.

    The tunnel is deliberately separate from HUD rendering.  Each gate is a
    desired vehicle pose in the trajectory's planning frame, so the same data
    can later feed a trajectory follower.  Width/height describe one constant
    docking aperture-sized visual frame; tolerances describe the allowed
    vehicle-centre window after hull dimensions/clearance are accounted for.
*/
struct GuidanceTunnelGate
{
    double distanceAlongTunnelMeters = 0.0;
    double sourceTrajectoryProgressMeters = 0.0;

    glm::dvec3 positionMeters {0.0};
    glm::dquat orientation {1.0, 0.0, 0.0, 0.0};

    double widthMeters = 0.0;
    double heightMeters = 0.0;
    double lateralToleranceMeters = 0.0;
    double verticalToleranceMeters = 0.0;
    double recommendedSpeedMps = 0.0;
};

struct GuidanceTunnel
{
    bool valid = false;
    int systemId = -1;
    std::string frameId;
    std::vector<GuidanceTunnelGate> gates;
    // Progress on the immutable trajectory that is already behind the live
    // ship when this dynamic corridor was rebuilt. No passed gate is emitted.
    double passedTrajectoryProgressMeters = 0.0;
    double maxCurvaturePerMeter = 0.0;
    double minimumTurnRadiusMeters = 0.0;
};

enum class GuidanceTunnelBuildMode
{
    // Sample the already accepted collision-safe trajectory. This is the
    // normal CALCULATE ROUTE path: HUD guidance must not invent a second
    // geometric route after trajectory planning has succeeded.
    TrajectoryBackbone,

    // Used only by an explicit deviation replan event. The current vehicle
    // pose becomes a new boundary condition and a smooth reconnect curve is
    // searched against canonical navigation obstacles.
    ReconnectCurrentPose
};

struct GuidanceTunnelRequest
{
    const Trajectory* trajectory = nullptr;
    GuidanceTunnelBuildMode buildMode =
        GuidanceTunnelBuildMode::ReconnectCurrentPose;

    glm::dvec3 currentPositionMeters {0.0};
    glm::dquat currentOrientation {1.0, 0.0, 0.0, 0.0};
    // Geometry follows actual motion when available. Hull attitude still drives
    // the gate orientation field independently. This matters in Newton flight.
    glm::dvec3 currentVelocityMps {0.0};

    glm::dvec3 terminalPositionMeters {0.0};
    glm::dquat terminalOrientation {1.0, 0.0, 0.0, 0.0};

    // Gates are equidistant by actual warped tunnel arc length.  The final
    // gate is always emitted even when the remaining distance is shorter.
    double gateSpacingMeters = 25.0;

    // One size for the complete tunnel, intentionally close to the usable
    // docking aperture rather than growing/shrinking along the route.
    double gateWidthMeters = 0.0;
    double gateHeightMeters = 0.0;
    double lateralToleranceMeters = 0.0;
    double verticalToleranceMeters = 0.0;

    // The near end follows the actual vehicle pose, then smoothly returns to
    // the immutable planned trajectory.  The far end smoothly follows the
    // live docking pose while retaining a locked terminal approach.
    double startCaptureDistanceMeters = 250.0;
    double terminalAlignmentDistanceMeters = 350.0;

    // Dynamic corridor regeneration uses the same canonical obstacle model as
    // route planning, so leaving the old centreline does not produce a pretty
    // but collision-cutting rejoin curve.
    std::vector<NavigationObstacle> obstacles;
    NavigationVehicleProfile vehicle;
    std::string terminalAllowedObstacleId;
    double terminalObstacleEntrySourceProgressMeters =
        std::numeric_limits<double>::infinity();

    double curveSampleSpacingMeters = 6.0;
    double curveChordErrorMeters = 0.05;
    std::size_t maxSmoothSupportLevel = 4;

    // Hard manual-guidance contract. Reconnect candidates may grow by several
    // kilometres or form a broad loop, but must not exceed this curvature.
    // Zero disables the bound.
    double minimumTurnRadiusMeters = 0.0;
};

class GuidanceTunnelBuilder
{
public:
    static GuidanceTunnel build(const GuidanceTunnelRequest& request);
};

} // namespace world::navigation
