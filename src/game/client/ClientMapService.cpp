#include "src/game/client/ClientMapService.h"

#include <type_traits>
#include <utility>

namespace game::client
{
ClientMapService::ClientMapService(ITransport& transport)
    : m_transport(transport)
{
}

void ClientMapService::pumpResponses()
{
    game::network::MapResponse response;

    while (m_transport.receiveMapResponse(response))
    {
        std::visit(
            [this](auto&& typedResponse)
            {
                using ResponseT = std::decay_t<decltype(typedResponse)>;

                if constexpr (std::is_same_v<
                                  ResponseT,
                                  game::network::GalaxyMapResponse>)
                {
                    if (typedResponse.requestId != m_galaxyRequestId)
                        return;

                    m_galaxyMetadata = typedResponse.metadata;
                    m_galaxySnapshot = std::move(typedResponse.snapshot);
                    m_hasGalaxy = true;
                    m_galaxyRequestId = 0;
                    m_galaxyResponseReady = true;
                }
                else if constexpr (std::is_same_v<
                                       ResponseT,
                                       game::network::SystemMapResponse>)
                {
                    if (typedResponse.requestId != m_systemRequestId ||
                        typedResponse.systemId != m_requestedSystemId)
                    {
                        return;
                    }

                    m_systemMetadata = typedResponse.metadata;
                    m_systemSnapshot = std::move(typedResponse.snapshot);
                    m_systemSnapshotId = typedResponse.systemId;
                    m_hasSystem = true;
                    m_systemRequestId = 0;
                    m_systemResponseReady = true;
                }
                else if constexpr (std::is_same_v<
                                       ResponseT,
                                       game::network::DetailMapResponse>)
                {
                    if (typedResponse.requestId != m_detailRequestId ||
                        typedResponse.target != m_requestedDetailTarget)
                    {
                        return;
                    }

                    m_detailMetadata = typedResponse.metadata;
                    m_detailSnapshot = std::move(typedResponse.snapshot);
                    m_detailSnapshotTarget = typedResponse.target;
                    m_hasDetail = true;
                    m_detailRequestId = 0;
                    m_detailResponseReady = true;
                }
                else if constexpr (std::is_same_v<
                                       ResponseT,
                                       game::network::HubMapResponse>)
                {
                    if (typedResponse.requestId != m_hubRequestId ||
                        typedResponse.systemId != m_requestedHubSystemId ||
                        typedResponse.hubId != m_requestedHubId)
                    {
                        return;
                    }

                    m_hubMetadata = typedResponse.metadata;
                    m_hubSnapshot = std::move(typedResponse.snapshot);
                    m_hubSnapshotSystemId = typedResponse.systemId;
                    m_hubSnapshotId = std::move(typedResponse.hubId);
                    m_hasHub = true;
                    m_hubRequestId = 0;
                    m_hubResponseReady = true;
                }
            },
            std::move(response));
    }
}

bool ClientMapService::requestGalaxy(bool forceRefresh)
{
    pumpResponses();
    if (m_galaxyResponseReady)
    {
        m_galaxyResponseReady = false;
        return m_hasGalaxy;
    }
    if (m_galaxyRequestId != 0)
        return false;
    if (!forceRefresh && m_hasGalaxy)
        return true;

    game::network::GalaxyMapRequest request;
    request.requestId = m_nextRequestId++;
    m_galaxyRequestId = request.requestId;
    m_transport.sendMapRequest(request);
    return false;
}

bool ClientMapService::requestSystem(int systemId, bool forceRefresh)
{
    pumpResponses();
    if (m_systemResponseReady && m_systemSnapshotId == systemId)
    {
        m_systemResponseReady = false;
        return true;
    }
    if (m_systemRequestId != 0)
        return false;
    if (!forceRefresh && m_hasSystem && m_systemSnapshotId == systemId)
        return true;

    game::network::SystemMapRequest request;
    request.requestId = m_nextRequestId++;
    request.systemId = systemId;
    m_systemRequestId = request.requestId;
    m_requestedSystemId = systemId;
    m_transport.sendMapRequest(request);
    return false;
}

bool ClientMapService::requestDetail(
    const world::celestial::DetailTarget& target,
    bool forceRefresh)
{
    if (!target.valid())
        return false;

    pumpResponses();
    if (m_detailResponseReady && m_detailSnapshotTarget == target)
    {
        m_detailResponseReady = false;
        return true;
    }
    if (m_detailRequestId != 0)
        return false;
    if (!forceRefresh && m_hasDetail && m_detailSnapshotTarget == target)
        return true;

    game::network::DetailMapRequest request;
    request.requestId = m_nextRequestId++;
    request.target = target;
    m_detailRequestId = request.requestId;
    m_requestedDetailTarget = target;
    m_transport.sendMapRequest(request);
    return false;
}

bool ClientMapService::requestHub(
    int systemId,
    const std::string& hubId,
    bool forceRefresh)
{
    if (systemId < 0 || hubId.empty())
        return false;

    pumpResponses();
    if (m_hubResponseReady &&
        m_hubSnapshotSystemId == systemId &&
        m_hubSnapshotId == hubId)
    {
        m_hubResponseReady = false;
        return true;
    }
    if (m_hubRequestId != 0)
        return false;
    if (!forceRefresh &&
        m_hasHub &&
        m_hubSnapshotSystemId == systemId &&
        m_hubSnapshotId == hubId)
    {
        return true;
    }

    game::network::HubMapRequest request;
    request.requestId = m_nextRequestId++;
    request.systemId = systemId;
    request.hubId = hubId;
    m_hubRequestId = request.requestId;
    m_requestedHubSystemId = systemId;
    m_requestedHubId = hubId;
    m_transport.sendMapRequest(request);
    return false;
}

const game::network::SnapshotMetadata&
ClientMapService::galaxyMetadata() const
{
    return m_galaxyMetadata;
}

const game::network::SnapshotMetadata&
ClientMapService::systemMetadata() const
{
    return m_systemMetadata;
}

const game::network::SnapshotMetadata&
ClientMapService::detailMetadata() const
{
    return m_detailMetadata;
}

const game::network::SnapshotMetadata&
ClientMapService::hubMetadata() const
{
    return m_hubMetadata;
}

const world::celestial::GalaxyMapSnapshot* ClientMapService::galaxy() const
{
    return m_hasGalaxy ? &m_galaxySnapshot : nullptr;
}

const world::celestial::SystemMapSnapshot*
ClientMapService::system(int systemId) const
{
    if (!m_hasSystem || m_systemSnapshotId != systemId)
        return nullptr;
    return &m_systemSnapshot;
}

const world::celestial::DetailMapSnapshot*
ClientMapService::detail(const world::celestial::DetailTarget& target) const
{
    if (!m_hasDetail || m_detailSnapshotTarget != target)
        return nullptr;
    return &m_detailSnapshot;
}

const world::celestial::HubMapSnapshot*
ClientMapService::hub(int systemId, const std::string& hubId) const
{
    if (!m_hasHub ||
        m_hubSnapshotSystemId != systemId ||
        m_hubSnapshotId != hubId)
    {
        return nullptr;
    }
    return &m_hubSnapshot;
}
}
