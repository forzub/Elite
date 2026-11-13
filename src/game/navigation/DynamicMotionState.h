#include "src/game/ship/telemetry/EngineLiveTelemetry.h"
#pragma once

#include <cstdint>

#include <array>
#include <cstddef>
#include <string>
#include <glm/glm.hpp>
#include "src/game/navigation/KinematicFrame.h"
#include "src/game/navigation/LocalFlightControlLaw.h"

namespace game::navigation
{

enum class MotionMode
{
    Inertial,
    Orbital,
    HubTactical,
    PassiveTrajectory,
    Cruise,
    JumpTransit,
    Docked,
    Reentry,
    Destroyed
};

struct DynamicMotionState
{
    MotionMode mode = MotionMode::Inertial;

    // Authoritative star-system membership for this dynamic entity.
    // WorldPosition is system-local; identical numeric meter coordinates in
    // different systems do not describe the same place.
    int systemId = -1;

    std::string parentBodyId;
    std::string hubId;

    // Authoritative moving frame owned by this ship. Local position/velocity
    // below are expressed in this frame, not directly in the hub frame.
    // While matchedToReferenceFrame is true the kinematics are refreshed from
    // the selected external frame (currently a hub). Later J propulsion will
    // detach and accelerate this frame without changing the local flight state.
    KinematicFrame travelFrame;
    bool matchedToReferenceFrame = false;
    std::string matchedReferenceFrameId;

    // Pilot control law for motion *inside* travelFrame. Ordinary/main
    // propulsion keeps the shared control envelope. The small body-axis RCS is
    // deliberately law-independent: Assisted bounds the resulting controlled
    // motion, while Newtonian may accumulate its tiny delta-v beyond the normal
    // speed envelope. External impulses remain physical in either law.
    LocalFlightControlLaw localControlLaw =
        defaultLocalFlightControlLaw();

    // Persistent velocity-vector alignment/autobrake action. HOME/INSERT/END
    // populate this through ShipControlState; ShipController and
    // DynamicMotionSystem execute it using normal ship limits.
    VelocityAlignmentMode velocityAlignmentMode =
        VelocityAlignmentMode::None;

    bool pendingReferenceVelocityMatch = false;
    // Скорость большой системы отсчёта:
    // орбита планеты, хаба, коридора и т.д.
    glm::dvec3 referenceVelocityMps {0.0};

    // Локальная позиция относительно текущего frame.
    glm::dvec3 localPositionMeters {0.0};

    // Локальная скорость относительно текущего frame.
    glm::dvec3 localVelocityMps {0.0};
    // Flight-assist слой.
    // Это не мировая инерционная скорость.
    // Это управляемые скорости корабля относительно текущего frame.
    double targetForwardSpeedMps = 0.0;

    // HOME in Assisted mode installs an explicit persistent max-speed
    // setpoint. Neutral +/- input must not immediately recapture the currently
    // reached speed on the next frame; any later +/- trim or END cancels it.
    bool assistedTargetSpeedHold = false;
    // Backspace mode: free hull rotation; no Assisted speed servo.
    bool assistedFreeRotation = false;

    // Ctrl+J free flight (server-owned, GameSimulationFreeFlight.cpp): the travel frame is
    // detached and carries the ship's GLOBAL (system / galaxy reference) motion under the real
    // gravity of all bodies; every tick the local motion is folded into it, so the 500 m/s local
    // envelope never caps the speed. +/- are direct main thrust, attitude and alignment use the
    // global velocity, turns under thrust are rate-limited.
    bool freeFlight = false;
    // HOME / END / INSERT in free flight: 0 thrust off, 1 full main thrust, 2 brake (flip and
    // burn against the velocity relative to the reference until it is ~0).
    std::uint8_t freeFlightThrust = 0;
    // The velocity the pilot flies against (HUD vector, Backspace exit alignment, brake): the
    // source hub's world velocity, updated by the server every tick. Speed shown = |v - ref|.
    std::string freeFlightReferenceId;
    glm::dvec3 freeFlightReferenceVelocityMps {0.0};
    // Last refused Ctrl+J OFF: 0 none, 1 no hub frame within the capture envelope,
    // 2 relative speed too high for a safe capture.
    std::uint8_t freeFlightCaptureRefusal = 0;
    // Free flight orbit relative to the dominant gravity body (server, every
    // tick): speed vs the body, radial component (+ climbing / - falling),
    // circular and escape speed at this radius, periapsis / apoapsis altitude
    // of the osculating orbit (bound only). Empty body id = no dominant body.
    std::string freeFlightOrbitBodyId;
    double freeFlightOrbitAltitudeM = 0.0;
    double freeFlightOrbitSpeedMps = 0.0;
    double freeFlightOrbitRadialMps = 0.0;
    double freeFlightOrbitCircularMps = 0.0;
    double freeFlightOrbitEscapeMps = 0.0;
    double freeFlightOrbitPeriapsisAltM = 0.0;
    double freeFlightOrbitApoapsisAltM = 0.0;
    bool freeFlightOrbitBound = false;
    // J near a body: minimum safe altitude (atmosphere top, airless: 0.1 % R),
    // dense layers, current atmospheric deceleration, impact counter (dense
    // layer entry / hard contact) and the surface state (0 flight, 1 landed,
    // 2 crashed).
    double freeFlightOrbitSafeAltM = 0.0;
    double freeFlightOrbitDenseAltM = 0.0;
    double freeFlightDragMps2 = 0.0;
    std::uint32_t freeFlightImpactSerial = 0;
    std::uint8_t freeFlightSurfaceState = 0;
    // the ship is destroyed (author 2026-11-08: "при ударе корабль разрушается"): 0 intact,
    // 1 a surface impact beyond survival, 2 the dense atmosphere at a deceleration beyond
    // survival (> 30 g); the hull is a wreck - no pilot, no control, no J, no jump
    std::uint8_t destroyedCause = 0;
    // the escape capsule at the destruction (navigation/rules/CapsuleRules.h capsule::State): 0 none,
    // 1 capsule intact - pilot alive, 2 capsule intact - pilot killed, 3 capsule destroyed, 4 no capsule;
    // capsuleBodyG = the deceleration the pilot took
    std::uint8_t capsuleState = 0;
    float capsuleBodyG = 0.0f;
    // terrain / sea surface height above the body radius under the ship
    // (the renderer's surface field; 0 until it is loaded or far above it)
    double freeFlightGroundHeightM = 0.0;
    // the ground under the ship: material (SurfaceMaterial; 100 water; -1 unknown),
    // slope and terrain feature (FeatureClass: 3 = impact crater, 2 = mountains ...)
    std::int16_t freeFlightGroundMaterial = -1;
    float freeFlightGroundSlopeDeg = 0.0f;
    std::uint8_t freeFlightGroundFeature = 0;
    // 1: no planet's sphere of influence holds the ship - the reference is the
    // star, planetary gravity negligible: global speed, no altitude (HUD dashes)
    std::uint8_t freeFlightReferenceIsRoot = 0;
    // CLIENT-side display only (not on the wire): the client's body autopilot / route
    // (SpaceStateBodyAutopilot.cpp overlay). Autopilot phase: 0 off, 1 departure, 2 transfer,
    // 3 approach, 4 descent, 5 down
    std::uint8_t freeFlightAutopilotPhase = 0;
    std::string freeFlightAutopilotTargetId;
    double freeFlightAutopilotRangeM = 0.0;        // to the target surface
    double freeFlightAutopilotRelSpeedMps = 0.0;   // relative to the target body
    float freeFlightAutopilotAccelMps2 = 0.0f;     // the autopilot's acceleration this tick
    // Flight architecture (author 2026-11-05 cc): J is an ENGINE (the J field's huge g in
    // place of the main engines' thrust), switched by the pilot only far from every object
    // and above a body's J floor; it exists only in NORMAL flight (freeFlight).
    bool jEngine = false;
    // what the velocity is measured against (HUD, the Assisted reference): 0 a hub
    // (stabilization in its frame), 1 space (normal flight, the star: global speed),
    // 2 a body (normal flight inside its sphere of influence, freeFlightReferenceId),
    // 3 orbit (stabilization on the ship's own ballistic trajectory), 4 ground
    // (stabilization against the turning surface, freeFlightOrbitBodyId)
    std::uint8_t velocityReferenceKind = 0;
    // the J engine: the pilot sets an ACCELERATION (signed, along the nose; HOME / END / INSERT,
    // +/- exponentially), the field follows it exponentially; main = the main engines' maximum
    double jAccelCommandMps2 = 0.0;
    double jAccelMps2 = 0.0;
    double mainEngineMaxMps2 = 0.0;
    bool jStopping = false;   // J switched off: the field winds down to the main engines' level first
    double jChargeT = -1.0;          // jump-drive charge left (t); < 0 = not yet filled (first J use)
    double jChargeCapacityT = 0.0;
    double shipMassT = 0.0;          // the ship's mass and its jump core's rating (the planner's charge rule)
    double jumpCoreRatingT = 0.0;
    // the interstellar jump (GameSimulationJump.cpp, navigation/rules/JumpRules.h): 0 none,
    // 1 charging, 2 charged - waiting clear of objects, 3 jumped - the exit being placed;
    // jumpSerial counts the jumps made (the client's ripple), jumpRefusalSerial the refusals
    std::uint8_t jumpState = 0;
    int jumpTargetSystemId = -1;
    std::string jumpTargetName;
    double jumpDistanceLy = 0.0;
    double jumpCostT = 0.0;
    double jumpChargeElapsedS = 0.0;
    double jumpChargeDurationS = 0.0;
    std::uint8_t jumpRefusal = 0;
    std::uint16_t jumpRefusalSerial = 0;
    std::uint16_t jumpSerial = 0;
    // the exit, once placed (state 3 -> 0): jumpExitSerial counts the exits (the client's "jump
    // complete" report); the deviation of the real exit from the calculated point and the planned
    // one-sigma error (JumpRules.h exitSigmaM), the exit body and the altitude above it
    std::uint16_t jumpExitSerial = 0;
    std::string jumpExitBodyName;
    double jumpExitDeviationM = 0.0;
    double jumpExitSigmaM = 0.0;
    double jumpExitAltitudeM = 0.0;
    // the lane records the ship carries (JUMP_MECHANICS 10h; InterstellarJumpRoute.h parseLaneRecords)
    std::string laneRecords;
    // the client's route (display only, not on the wire): 0 none, 1 flying, 2 paused (manual flight
    // until RESUME), 3 a service stay, 4 landing (the landing autopilot); legIndex / legCount of it
    std::uint8_t routeState = 0;
    std::uint8_t routeLegIndex = 0;
    std::uint8_t routeLegCount = 0;
    // navigation-zone violations, judged by the SERVER (route_zones.json): the total of the fines,
    // a serial bumped on every new violation, the last one (zone id, 1 red entry / 2 gate skipped)
    double zoneFinesTotalCr = 0.0;
    std::uint16_t zoneViolationSerial = 0;
    std::uint8_t zoneViolationKind = 0;
    std::string zoneViolationZoneId;

    // True only while/after a real +/- Assisted trim. It lets the release edge
    // capture reached forward speed once without allowing keypad RCS or an
    // external impulse to silently rewrite the longitudinal setpoint.
    bool assistedThrottleTrimWasActive = false;

    double forwardSpeedMps = 0.0;
    double strafeSpeedMps = 0.0;
    double liftSpeedMps = 0.0;

    // Longitudinal main-thrust request and the independent body-axis
    // manoeuvre/RCS request. They stay separate until the fixed kinematic step.
    //
    // Direction is a hardware fact from ShipParams, not a control-law fact.
    // A vehicle with only an aft/rear main source can accelerate only along
    // ship forward; reverse main acceleration requires a physically installed
    // fore/reverse source. Assisted/Newtonian may command the hardware
    // differently, but neither law may invent an actuator.
    // Command / Request / Executed boundary (migration seam).
    // The flight law requests force in these three channels. The actuator
    // executor records them here BEFORE replacing the legacy motion fields
    // with physically delivered acceleration. Consumers of actual motion
    // must never use these request values to advance position.
    glm::dvec3 requestedMainEngineAccelerationMps2 {0.0};
    glm::dvec3 requestedManoeuvreAccelerationMps2 {0.0};
    glm::dvec3 requestedAssistedStabilizationAccelerationMps2 {0.0};

    // Execution provenance for the CURRENT fixed step only. When true the
    // installed main-engine plant has supplied actual force/mass, and the
    // motion integrator must not post-clamp that physical impulse. Cleared
    // after propagation; no prediction or controller may treat it as
    // a permanent capability. Legacy diagnostics use false.
    bool installedMainPlantExecuted = false;
    // Real Newtonian 24-nozzle executor owns the gas accumulator and has
    // already produced actual force/mass. Never apply legacy RCS gas cutoff
    // to those delivered forces a second time during integration.
    bool installedRcsPlantExecuted = false;

    // Legacy integrated motion channels: after the physical gateway these
    // contain EXECUTED force/mass, never requested force/mass.
    glm::dvec3 mainEngineAccelerationMps2 {0.0};
    glm::dvec3 manoeuvreAccelerationMps2 {0.0};

    // Newtonian manual-RCS delta-v ledger in the travel frame. This tracks
    // ONLY velocity change physically produced by keypad manoeuvre thrusters.
    // When all manual manoeuvre axes are released, the same RCS hardware
    // applies opposite bounded thrust until this ledger returns to zero.
    // Main-engine delta-v, pre-existing drift and external impulses are not
    // included and therefore are not cancelled by manual-RCS stabilization.
    glm::dvec3 newtonianManualManoeuvreDeltaVelocityMps {0.0};
    bool newtonianManualManoeuvreInputActive = false;
    // Current command owns its RCS delta-v (ShipControlState::
    // rcsKeepsTranslationDeltaV): the ledger stays empty, nothing is released.
    bool newtonianRcsKeepsDeltaV = false;
    // Translation inputs are per-direction normalized
    // (ShipControlState::rcsTranslationPerAxisAuthority).
    bool newtonianRcsPerAxisAuthority = false;

    // Newtonian main-nozzle TVC. Commands are normalized pilot inputs;
    // actual angles are rate-limited physical actuator state. Assisted ignores
    // this channel, so adding it cannot alter Assisted translation behavior.
    double mainGimbalRightCommand01 = 0.0;
    double mainGimbalUpCommand01 = 0.0;
    double mainGimbalYawRad = 0.0;
    double mainGimbalPitchRad = 0.0;

    // Assisted-only automatic lateral stabilization. This is distinct from
    // the pilot's gas-limited keypad RCS command above. Its capability comes
    // from ShipParams::strafeAccel through the central ShipDynamics adapter;
    // Newtonian never applies this demand.
    glm::dvec3 assistedStabilizationAccelerationMps2 {0.0};

    // Applied combined propulsion acceleration. Installed main-engine forces
    // are request-limited BEFORE physical execution, never post-clipped here;
    // the legacy non-plant path retains its historical speed-envelope rule.
    glm::dvec3 engineAccelerationMps2 {0.0};

    // Rigid-body reaction of the currently firing physical main slots. These
    // are diagnostics/state from the physical plant, not controller demands.
    glm::dvec3 mainPropulsionTorqueBodyNm {0.0};
    glm::dvec3 mainPropulsionAngularAccelerationBodyRadPerSec2 {0.0};

    // Damaged-main feedback diagnostics. Healthy symmetric propulsion remains
    // load=0 / usable=1. Both Assisted and Newtonian may reduce individual
    // main-engine commands and use small automatic TVC trim when that makes
    // the requested maneuver cleaner. Residual r x F reaction remains physical.
    double mainTrimLoad01 = 0.0;
    double mainTrimUsableScale01 = 1.0;

    // Latest allocator-produced DELIVERED nominal-thrust fraction for each
    // physical slot. Held between control ticks for the systems simulation.
    // Zero for the inactive front/rear bank; no dynamic per-tick allocations.
    std::array<double, 16> mainEngineDeliveredThrust01 {};
    std::size_t mainEngineReportedSlotCount = 0;

    // Independent, factual actuator telemetry for cockpits/replay. The same
    // state also holds the previous individual nozzle orientations, allowing
    // slew-rate-limited TVC across successive control samples.
    game::ship::telemetry::EngineLiveTelemetry engineTelemetry {};
    glm::dvec3 requestedMainAngularAccelerationBodyRadPerSec2 {0.0};

    // Factual damage-dependent propulsion reference for cockpit and flight
    // control. Body +X right/+Y up/-Z forward; -Z is the healthy default.
    bool assistedWorkingThrustAxisEmergency = false;
    glm::dvec3 assistedWorkingThrustAxisBody {0.0, 0.0, -1.0};

    // 0..1 normalized pressure in the replenished manoeuvre-gas accumulator.
    double manoeuvreGasPressure01 = 1.0;
    bool manoeuvreGasDepleted = false;

    // Диагностика: желаемая локальная тактическая скорость,
    // не глобальная скорость корабля.
    glm::dvec3 desiredTacticalVelocityMps {0.0};

    glm::dvec3 worldVelocityMps {0.0};

    // Локальная желаемая скорость относительно навигационного ориентира.
    // Это НЕ вся глобальная скорость корабля.
    glm::dvec3 desiredRelativeVelocityMps {0.0};

    // Автовыравнивание по орбитальному коридору.
    // Автовыравнивание по орбитальному коридору.
    // Пока выключено. Это будет использоваться позже,
    // когда игрок промахнулся мимо орбитального туннеля.
    bool orbitalAssistEnabled = false;

    double orbitalAssistMaxAngleDeg = 30.0;
    double orbitalAssistStrength = 0.35;

    // Гравитационный слой.
    glm::dvec3 gravityAccelerationMps2 {0.0};

    std::string primaryGravityBodyId;
    double primaryGravityDistanceMeters = 0.0;
    double primaryGravityAltitudeMeters = 0.0;
    double primaryGravityAccelerationMps2 = 0.0;

    // Орбитальный коридор.
    std::string orbitalCorridorId;
    int orbitalCorridorState = 0;

    double orbitalAltitudeMeters = 0.0;
    double orbitalAltitudeErrorMeters = 0.0;
    double orbitalTargetSpeedMps = 0.0;
    double orbitalTangentialSpeedMps = 0.0;
    double orbitalRadialSpeedMps = 0.0;
    double orbitalSpeedErrorMps = 0.0;

    // Будущий Cruise/Jump слой.

    // Орбитальный слой.
    double altitudeMeters = 0.0;
    double orbitalPhaseRadians = 0.0;
    double planeOffsetMeters = 0.0;

    bool lockedToFramePosition = false;
};

} // namespace game::navigation