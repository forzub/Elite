#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "src/game/navigation/HubSemanticAnchor.h"
#include "src/game/navigation/NavigationWorldPredictor.h"
#include "src/game/simulation/HubAttachmentSnapshot.h"
#include "src/world/navigation/NavigationObstacle.h"
#include "src/world/navigation/NavigationVehicleProfile.h"
#include "src/world/navigation/Trajectory.h"
#include "src/world/navigation/GuidanceTunnel.h"

namespace game::navigation
{

/*
    Immutable route/trajectory plus one fixed Hub-local manual corridor.

    The corridor is rebuilt only on an explicit guidance replan event (initial
    route calculation or leaving the safe centre envelope). Between replans the
    ship moves through stationary gates; presentation only removes passed gates
    and transforms the remaining Hub-local geometry into the current render frame.
*/
struct ManualDockingGuidancePlan
{
    bool valid = false;
    std::uint64_t requestSerial = 0;
    int systemId = -1;
    std::string corridorId;

    HubPredictionSource hubPredictionSource;
    game::simulation::HubAttachmentSnapshot targetAttachment;
    HubSemanticAnchorDefinition targetAnchor;
    world::navigation::Trajectory trajectory;
    world::navigation::GuidanceTunnel fixedTunnel;
    std::size_t firstActiveGateIndex = 0;
    double passedTunnelDistanceMeters = 0.0;
    double lastReplanServerTimeSeconds = -1.0e30;
    std::vector<world::navigation::NavigationObstacle> obstacles;
    world::navigation::NavigationVehicleProfile vehicle;
    std::string targetObstacleId;
    double terminalObstacleEntrySourceProgressMeters = 0.0;

    double gateSpacingMeters = 25.0;
    double gateWidthMeters = 0.0;
    double gateHeightMeters = 0.0;
    double lateralToleranceMeters = 0.0;
    double verticalToleranceMeters = 0.0;
    double startCaptureDistanceMeters = 250.0;
    double terminalAlignmentDistanceMeters = 350.0;
    double transitMaxClosureRateMps = 0.0;
    double dockingMaxClosureRateMps = 0.0;

    // Rolling manual guidance is still synchronous on the client frame. A
    // failed/expensive reconnect previously took longer than the old 0.25 s
    // period, so the next frame was already overdue and immediately launched
    // another solve. Until this solve is moved to a worker/latest-wins job,
    // 1 Hz is the hard protection against frame-by-frame replan thrash.
    double replanCheckIntervalSeconds = 1.0;
    double nextReplanCheckServerTimeSeconds = -1.0e30;
    double predictedLookAheadSeconds = 0.75;
    double preemptiveToleranceScale = 0.65;

    // IMPORTANT: the old hard-envelope path bypassed the cadence timer and
    // launched a synchronous reconnect immediately. When one reconnect took
    // ~0.37 s, that became a frame-by-frame replan storm. Manual guidance is
    // advisory, so while reconnect remains synchronous all rebuild triggers
    // must go through the 1 Hz policy above. A future worker/latest-wins
    // implementation can restore an immediate hard-envelope event safely.
    double hardToleranceScale = 1.0e9;

    double courseChangeThresholdRadians = 0.06981317007977318; // 4 deg
    double targetPositionReplanMeters = 8.0;
    double targetAngleReplanRadians = 0.02617993877991494; // 1.5 deg
    double minimumVisualTurnRadiusMeters = 1500.0;
    std::uint64_t tunnelGeneration = 0;
};

} // namespace game::navigation
