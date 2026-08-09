#pragma once

#include <cstdint>

#include "src/game/entity/EntityType.h"
#include "src/game/motion/MotionModel.h"
#include "src/game/simulation/AuthorityPolicy.h"
#include "src/game/simulation/SimulationMode.h"
#include "src/game/simulation/TimelineDomain.h"

namespace game::simulation
{

struct EntityRuntimeContract
{
    game::entity::EntityType entityType = game::entity::EntityType::Unknown;
    game::motion::MotionModel motionModel = game::motion::MotionModel::Static;
    SimulationMode simulationMode = SimulationMode::Dormant;
    AuthorityPolicy authority = AuthorityPolicy::ServerAuthoritative;
    TimelineDomain timeline = TimelineDomain::None;
};

enum class RuntimeContractError : std::uint8_t
{
    None = 0,
    UnknownEntityType,
    IllegalEntityMotionPair,
    IllegalSimulationModeForMotion,
    IllegalTimelineForMotion,
    IllegalAuthorityForEntity,
    ClientPredictionRequiresActiveDynamicShip
};

constexpr bool isAnalyticMotion(game::motion::MotionModel motion) noexcept
{
    using game::motion::MotionModel;
    return
        motion == MotionModel::Static ||
        motion == MotionModel::Orbital ||
        motion == MotionModel::HubAttached ||
        motion == MotionModel::Kinematic ||
        motion == MotionModel::ScheduledTrajectory ||
        motion == MotionModel::FleetFormation;
}

constexpr bool entityAllowsMotion(
    game::entity::EntityType entity,
    game::motion::MotionModel motion
) noexcept
{
    using game::entity::EntityType;
    using game::motion::MotionModel;

    switch (entity)
    {
        case EntityType::Ship:
            return
                motion == MotionModel::Static ||
                motion == MotionModel::HubAttached ||
                motion == MotionModel::ScheduledTrajectory ||
                motion == MotionModel::FleetFormation ||
                motion == MotionModel::DynamicPhysics;

        case EntityType::CelestialBody:
            return
                motion == MotionModel::Static ||
                motion == MotionModel::Orbital;

        case EntityType::Hub:
            return motion == MotionModel::Orbital;

        case EntityType::StationModule:
            return
                motion == MotionModel::HubAttached ||
                motion == MotionModel::Kinematic;

        case EntityType::Asteroid:
            return
                motion == MotionModel::Static ||
                motion == MotionModel::Orbital ||
                motion == MotionModel::DynamicPhysics;

        case EntityType::Projectile:
            return motion == MotionModel::DynamicPhysics;

        case EntityType::FleetAggregate:
            return motion == MotionModel::ScheduledTrajectory;

        case EntityType::DiagnosticProbe:
            return
                motion == MotionModel::Static ||
                motion == MotionModel::Kinematic ||
                motion == MotionModel::ScheduledTrajectory;

        case EntityType::Unknown:
            return false;
    }

    return false;
}

constexpr bool simulationModeAllowsMotion(
    SimulationMode mode,
    game::motion::MotionModel motion
) noexcept
{
    using game::motion::MotionModel;

    switch (motion)
    {
        case MotionModel::Static:
            return
                mode == SimulationMode::Dormant ||
                mode == SimulationMode::OnDemand ||
                mode == SimulationMode::Prewarm ||
                mode == SimulationMode::Active;

        case MotionModel::Orbital:
        case MotionModel::HubAttached:
        case MotionModel::Kinematic:
            return
                mode == SimulationMode::OnDemand ||
                mode == SimulationMode::Prewarm ||
                mode == SimulationMode::Active;

        case MotionModel::ScheduledTrajectory:
        case MotionModel::FleetFormation:
            return
                mode == SimulationMode::Scheduled ||
                mode == SimulationMode::Coarse ||
                mode == SimulationMode::Prewarm;

        case MotionModel::DynamicPhysics:
            return
                mode == SimulationMode::Coarse ||
                mode == SimulationMode::Prewarm ||
                mode == SimulationMode::Active;
    }

    return false;
}

constexpr bool timelineAllowsMotion(
    TimelineDomain timeline,
    game::motion::MotionModel motion
) noexcept
{
    using game::motion::MotionModel;

    switch (motion)
    {
        case MotionModel::Static:
            return timeline == TimelineDomain::None;

        case MotionModel::Orbital:
        case MotionModel::HubAttached:
            return timeline == TimelineDomain::Universe;

        case MotionModel::Kinematic:
        case MotionModel::ScheduledTrajectory:
        case MotionModel::FleetFormation:
        case MotionModel::DynamicPhysics:
            return timeline == TimelineDomain::ServerSimulation;
    }

    return false;
}

constexpr RuntimeContractError validateRuntimeContract(
    const EntityRuntimeContract& contract
) noexcept
{
    using game::entity::EntityType;
    using game::motion::MotionModel;

    if (contract.entityType == EntityType::Unknown)
        return RuntimeContractError::UnknownEntityType;

    if (!entityAllowsMotion(contract.entityType, contract.motionModel))
        return RuntimeContractError::IllegalEntityMotionPair;

    if (!simulationModeAllowsMotion(
            contract.simulationMode,
            contract.motionModel))
    {
        return RuntimeContractError::IllegalSimulationModeForMotion;
    }

    if (!timelineAllowsMotion(contract.timeline, contract.motionModel))
        return RuntimeContractError::IllegalTimelineForMotion;

    if (contract.authority == AuthorityPolicy::PresentationOnly)
    {
        if (contract.entityType != EntityType::DiagnosticProbe)
            return RuntimeContractError::IllegalAuthorityForEntity;
    }
    else if (contract.entityType == EntityType::DiagnosticProbe)
    {
        return RuntimeContractError::IllegalAuthorityForEntity;
    }

    if (contract.authority == AuthorityPolicy::ServerAuthoritativeWithClientPrediction)
    {
        if (
            contract.entityType != EntityType::Ship ||
            contract.motionModel != MotionModel::DynamicPhysics ||
            contract.simulationMode != SimulationMode::Active)
        {
            return RuntimeContractError::ClientPredictionRequiresActiveDynamicShip;
        }
    }

    return RuntimeContractError::None;
}

constexpr bool isRuntimeContractValid(
    const EntityRuntimeContract& contract
) noexcept
{
    return validateRuntimeContract(contract) == RuntimeContractError::None;
}

constexpr bool canTransitionSimulationMode(
    SimulationMode from,
    SimulationMode to
) noexcept
{
    if (from == to)
        return true;

    switch (from)
    {
        case SimulationMode::Dormant:
            return
                to == SimulationMode::OnDemand ||
                to == SimulationMode::Scheduled ||
                to == SimulationMode::Prewarm;

        case SimulationMode::OnDemand:
            return
                to == SimulationMode::Dormant ||
                to == SimulationMode::Prewarm ||
                to == SimulationMode::Active;

        case SimulationMode::Scheduled:
            return
                to == SimulationMode::Dormant ||
                to == SimulationMode::Coarse ||
                to == SimulationMode::Prewarm;

        case SimulationMode::Coarse:
            return
                to == SimulationMode::Dormant ||
                to == SimulationMode::Scheduled ||
                to == SimulationMode::Prewarm ||
                to == SimulationMode::Active;

        case SimulationMode::Prewarm:
            return
                to == SimulationMode::Dormant ||
                to == SimulationMode::OnDemand ||
                to == SimulationMode::Scheduled ||
                to == SimulationMode::Coarse ||
                to == SimulationMode::Active;

        case SimulationMode::Active:
            return
                to == SimulationMode::Prewarm ||
                to == SimulationMode::Coarse;
    }

    return false;
}

} // namespace game::simulation
