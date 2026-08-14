#pragma once

#include <string>
#include <glm/glm.hpp>
#include "src/game/navigation/NavigationPlan.h"
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

    // Pilot control law for motion *inside* travelFrame. Both laws obey the
    // same propulsion/control envelope. External impulses remain physical and
    // may push linear/angular motion beyond those normal control limits.
    // The laws differ only in how pilot input becomes engine acceleration.
    LocalFlightControlLaw localControlLaw =
        LocalFlightControlLaw::Newtonian;

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

    double forwardSpeedMps = 0.0;
    double strafeSpeedMps = 0.0;
    double liftSpeedMps = 0.0;

    // Tactical engine intent.
    // Это ускорение от двигателей в мировых координатах.
    // Оно меняет worldVelocityMps, но не заменяет его.
    glm::dvec3 engineAccelerationMps2 {0.0};

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
    NavigationPlan navigationPlan;

    // Орбитальный слой.
    double altitudeMeters = 0.0;
    double orbitalPhaseRadians = 0.0;
    double planeOffsetMeters = 0.0;

    bool lockedToFramePosition = false;
};

} // namespace game::navigation