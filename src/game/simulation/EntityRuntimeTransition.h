#pragma once

#include "src/game/simulation/EntityRuntimeContract.h"

namespace game::simulation
{

enum class RuntimeTransitionError : std::uint8_t
{
    None = 0,
    InvalidSourceContract,
    InvalidTargetContract,
    EntityTypeChanged,
    AuthorityChanged,
    IllegalSimulationModeTransition,
    IllegalMotionModelTransition
};

constexpr bool canTransitionMotionModel(
    game::entity::EntityType entityType,
    game::motion::MotionModel from,
    game::motion::MotionModel to
) noexcept
{
    using game::entity::EntityType;
    using game::motion::MotionModel;

    if (from == to)
        return true;

    switch (entityType)
    {
        case EntityType::Ship:
            return
                (from == MotionModel::ScheduledTrajectory &&
                 to == MotionModel::DynamicPhysics) ||
                (from == MotionModel::DynamicPhysics &&
                 to == MotionModel::ScheduledTrajectory) ||
                (from == MotionModel::FleetFormation &&
                 to == MotionModel::DynamicPhysics) ||
                (from == MotionModel::DynamicPhysics &&
                 to == MotionModel::FleetFormation) ||
                (from == MotionModel::HubAttached &&
                 to == MotionModel::DynamicPhysics) ||
                (from == MotionModel::DynamicPhysics &&
                 to == MotionModel::HubAttached);

        case EntityType::Asteroid:
            return
                ((from == MotionModel::Static ||
                  from == MotionModel::Orbital) &&
                 to == MotionModel::DynamicPhysics) ||
                (from == MotionModel::DynamicPhysics &&
                 (to == MotionModel::Static ||
                  to == MotionModel::Orbital));

        case EntityType::StationModule:
            return
                (from == MotionModel::HubAttached &&
                 to == MotionModel::Kinematic) ||
                (from == MotionModel::Kinematic &&
                 to == MotionModel::HubAttached);

        case EntityType::DiagnosticProbe:
            return entityAllowsMotion(entityType, from) &&
                   entityAllowsMotion(entityType, to);

        case EntityType::Unknown:
        case EntityType::CelestialBody:
        case EntityType::Hub:
        case EntityType::Projectile:
        case EntityType::FleetAggregate:
            return false;
    }

    return false;
}

constexpr RuntimeTransitionError validateRuntimeTransition(
    const EntityRuntimeContract& from,
    const EntityRuntimeContract& to
) noexcept
{
    if (!isRuntimeContractValid(from))
        return RuntimeTransitionError::InvalidSourceContract;

    if (!isRuntimeContractValid(to))
        return RuntimeTransitionError::InvalidTargetContract;

    if (from.entityType != to.entityType)
        return RuntimeTransitionError::EntityTypeChanged;

    if (from.authority != to.authority)
        return RuntimeTransitionError::AuthorityChanged;

    if (!canTransitionSimulationMode(from.simulationMode, to.simulationMode))
        return RuntimeTransitionError::IllegalSimulationModeTransition;

    if (!canTransitionMotionModel(
            from.entityType,
            from.motionModel,
            to.motionModel))
    {
        return RuntimeTransitionError::IllegalMotionModelTransition;
    }

    return RuntimeTransitionError::None;
}

constexpr bool isRuntimeTransitionValid(
    const EntityRuntimeContract& from,
    const EntityRuntimeContract& to
) noexcept
{
    return validateRuntimeTransition(from, to) == RuntimeTransitionError::None;
}

} // namespace game::simulation
