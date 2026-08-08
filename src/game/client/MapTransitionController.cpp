#include "src/game/client/MapTransitionController.h"

#include <utility>

namespace game::client
{
void MapTransitionController::beginSystem(int systemId)
{
    clear();
    m_kind = Kind::System;
    m_systemId = systemId;
}

void MapTransitionController::beginDetail(
    const world::celestial::DetailTarget& target)
{
    clear();
    m_kind = Kind::Detail;
    m_detailTarget = target;
}

void MapTransitionController::beginHub(int systemId, std::string hubId)
{
    clear();
    m_kind = Kind::Hub;
    m_systemId = systemId;
    m_hubId = std::move(hubId);
}

void MapTransitionController::clear()
{
    m_kind = Kind::None;
    m_systemId = -1;
    m_detailTarget = {};
    m_hubId.clear();
}

bool MapTransitionController::pending() const
{
    return m_kind != Kind::None;
}

MapTransitionController::Kind MapTransitionController::kind() const
{
    return m_kind;
}

int MapTransitionController::systemId() const
{
    return m_systemId;
}

const world::celestial::DetailTarget&
MapTransitionController::detailTarget() const
{
    return m_detailTarget;
}

const std::string& MapTransitionController::hubId() const
{
    return m_hubId;
}

bool MapTransitionController::requestFailed(ClientRequestStatus status)
{
    return status == ClientRequestStatus::TimedOut ||
           status == ClientRequestStatus::Failed ||
           status == ClientRequestStatus::Cancelled;
}

bool MapTransitionController::simulationHasReached(
    const game::network::SnapshotMetadata& mapMetadata,
    const game::network::SnapshotMetadata& simulationMetadata)
{
    if (mapMetadata.universeTimelineRevision !=
        simulationMetadata.universeTimelineRevision)
    {
        return false;
    }

    return mapMetadata.serverTick == 0 ||
           simulationMetadata.serverTick >= mapMetadata.serverTick;
}
}
