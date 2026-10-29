#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>
#include <glm/glm.hpp>
#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/AcceptedManeuverProgramBuilder.h"
#include "src/game/navigation/ManeuverCapabilityAdapters.h"
#include "src/game/navigation/NavigationControlCapability.h"
#include "src/game/navigation/ManeuverTrackingController.h"
#include "src/game/navigation/DockingAutomaticRecoveryPolicy.h"
#include "src/game/navigation/RouteFrameField.h"
#include "src/game/navigation/TwoPointRollGeometry.h"
#include "src/game/navigation/autopilot/CourseCaptureGuidance.h"
#include "src/game/navigation/AssistedConcurrentManeuverBudget.h"
#include "src/game/navigation/autopilot/InCorridorCenteringGuidance.h"
#include "src/game/navigation/autopilot/HullPoseGuidance.h"
#include "src/game/navigation/autopilot/HullTunnelRollGuidance.h"
#include "src/game/navigation/autopilot/AutopilotControlApi.h"
#include "src/game/navigation/autopilot/NewtonianManoeuvreProgram.h"
#include "src/game/navigation/autopilot/NewtonianSpeedTolerancePolicy.h"
#include "src/game/navigation/autopilot/RouteSpeedGuidance.h"
#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/navigation/autopilot/NewtonianCapabilityWatch.h"
#include "src/world/navigation/TrajectoryGenerator.h"
#include "src/world/navigation/NavigationVehicleProfile.h"

namespace game::navigation::autopilot
{

struct AutopilotTrackingPolicy
{
    double positionGainPerSecond2 = 0.50;
    double velocityGainPerSecond = 1.00;
    double attitudeGainPerSecond2 = 2.00;
    double angularVelocityGainPerSecond = 3.00;
};

struct AutopilotAgentState
{
    glm::dvec3 positionMapMeters {0.0};
    glm::dvec3 velocityMapMetersPerSecond {0.0};
    glm::dvec3 forwardMap {0.0, 0.0, -1.0};
    glm::dvec3 rightMap {1.0, 0.0, 0.0};
    glm::dvec3 upMap {0.0, 1.0, 0.0};
    double pitchRateRadPerSec = 0.0;
    double yawRateRadPerSec = 0.0;
    double rollRateRadPerSec = 0.0;
};

// Route execution state shared by both route-autopilot laws (everything but
// the pilot controller, which the DedicatedClientRouteAutopilot template adds).
struct RouteExecutionState
{
    bool active = false;
    std::uint64_t requestSerial = 0;
    std::size_t currentPage = 0;
    std::size_t currentSpatialSegment = 0;

    // AcceptedManeuverProgram pages are fixed-capacity storage only.
    // Execution owns one continuous spatial reference assembled from all
    // pages, with their duplicated boundary sample removed.
    std::vector<AcceptedManeuverProgram::ReferenceSample>
        continuousSamples;
    std::vector<double> continuousProgressMeters;
    std::vector<double> nominalSpeedProfileMps;
    // The nominal profile as planned (before any capability reprofile).
    std::vector<double> plannedNominalSpeedProfileMps;
    std::vector<double> runtimeSpeedProfileMps;
    std::vector<double> runtimeLongitudinalAccelerationMps2;
    std::vector<double> checkpointProgressMeters;
    std::size_t currentContinuousSegment = 0;
    std::size_t nextCheckpointIndex = 1;
    std::uint64_t speedProfileRevision = 0;
    // Newtonian: a speed deviation beyond the tolerance band is being
    // corrected (NewtonianSpeedTolerancePolicy hysteresis).
    bool speedCorrectionActive = false;
    bool holdAtTerminal = false;
    double trackingPositionToleranceMeters = 0.0;

    // Authoritative spatial geometry. Accepted maneuver samples still own
    // timing/speed/attitude feed-forward, but they no longer define the
    // route shape when Planner supplied parametric curves.
    std::vector<planner::RouteCurveSegment> routeCurves;
    std::vector<double> curveSampleStartProgressMeters;
    std::vector<double> curveSampleEndProgressMeters;
    glm::dvec3 routeUpReference {0.0};

    // Same canonical frame field used by tunnel presentation. The hull
    // must align to this field, never to an independently reconstructed
    // "up" convention.
    std::vector<RouteFrameField::Sample> routeFrameField;
    // Fixed-attitude stretches (Step 3), route progress.
    std::vector<planner::RouteNarrowZone> narrowZones;
    double liveTunnelRollPhaseRad = 0.0;
    double previousLiveTunnelRollPhaseRad = 0.0;
    double liveTunnelRollRateRadPerSec = 0.0;
    bool hasLiveTunnelRollPhase = false;

    AutopilotTrackingPolicy followerPolicy {};
    std::vector<AcceptedManeuverProgram> programs;
    // Newtonian: the capability the active program was accepted with.
    NewtonianCapabilityWatch capabilityWatch {};
};

} // namespace game::navigation::autopilot
