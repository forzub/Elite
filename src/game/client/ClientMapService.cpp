#include "src/game/client/ClientMapService.h"

#include <algorithm>
#include <type_traits>
#include <utility>

namespace game::client
{
ClientMapService::ClientMapService(ITransport& transport)
    : m_transport(transport)
{
}

std::uint64_t ClientMapService::nextRequestId()
{
    return m_nextRequestId++;
}

void ClientMapService::begin(RequestState& state, std::uint64_t requestId)
{
    state.status = ClientRequestStatus::Pending;
    state.requestId = requestId;
    state.elapsedSeconds = 0.0f;
    ++state.attempts;
}

void ClientMapService::complete(RequestState& state)
{
    state.status = ClientRequestStatus::Ready;
    state.requestId = 0;
    state.elapsedSeconds = 0.0f;
    state.attempts = 0;
}

void ClientMapService::cancel(RequestState& state)
{
    if (state.status == ClientRequestStatus::Pending)
        state.status = ClientRequestStatus::Cancelled;
    state.requestId = 0;
    state.elapsedSeconds = 0.0f;
    state.attempts = 0;
}

bool ClientMapService::advanceTimeout(RequestState& state, float dt)
{
    if (state.status != ClientRequestStatus::Pending)
        return false;

    state.elapsedSeconds += std::max(dt, 0.0f);
    if (state.elapsedSeconds < RequestTimeoutSeconds)
        return false;

    state.elapsedSeconds = 0.0f;
    if (state.attempts >= MaxRequestAttempts)
    {
        state.status = ClientRequestStatus::TimedOut;
        state.requestId = 0;
        return false;
    }

    return true;
}

void ClientMapService::sendGalaxyRequest()
{
    game::network::GalaxyMapRequest request;
    request.requestId = nextRequestId();
    begin(m_galaxyRequest, request.requestId);
    m_transport.sendMapRequest(request);
}

void ClientMapService::sendSystemRequest()
{
    game::network::SystemMapRequest request;
    request.requestId = nextRequestId();
    request.systemId = m_requestedSystemId;
    begin(m_systemRequest, request.requestId);
    m_transport.sendMapRequest(request);
}

void ClientMapService::sendDetailRequest()
{
    game::network::DetailMapRequest request;
    request.requestId = nextRequestId();
    request.target = m_requestedDetailTarget;
    begin(m_detailRequest, request.requestId);
    m_transport.sendMapRequest(request);
}

void ClientMapService::sendHubRequest()
{
    game::network::HubMapRequest request;
    request.requestId = nextRequestId();
    request.systemId = m_requestedHubSystemId;
    request.hubId = m_requestedHubId;
    begin(m_hubRequest, request.requestId);
    m_transport.sendMapRequest(request);
}

void ClientMapService::update(float dt)
{
    pumpResponses();

    if (advanceTimeout(m_galaxyRequest, dt))
        sendGalaxyRequest();
    if (advanceTimeout(m_systemRequest, dt))
        sendSystemRequest();
    if (advanceTimeout(m_detailRequest, dt))
        sendDetailRequest();
    if (advanceTimeout(m_hubRequest, dt))
        sendHubRequest();
}

void ClientMapService::resetPendingRequests()
{
    cancel(m_galaxyRequest);
    cancel(m_systemRequest);
    cancel(m_detailRequest);
    cancel(m_hubRequest);
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

                if constexpr (std::is_same_v<ResponseT, game::network::GalaxyMapResponse>)
                {
                    if (typedResponse.requestId != m_galaxyRequest.requestId)
                        return;
                    m_galaxyMetadata = typedResponse.metadata;
                    m_galaxySnapshot = std::move(typedResponse.snapshot);
                    m_hasGalaxy = true;
                    complete(m_galaxyRequest);
                }
                else if constexpr (std::is_same_v<ResponseT, game::network::SystemMapResponse>)
                {
                    if (typedResponse.requestId != m_systemRequest.requestId ||
                        typedResponse.systemId != m_requestedSystemId)
                        return;
                    m_systemMetadata = typedResponse.metadata;
                    m_systemSnapshot = std::move(typedResponse.snapshot);
                    m_systemSnapshotId = typedResponse.systemId;
                    m_hasSystem = true;
                    complete(m_systemRequest);
                }
                else if constexpr (std::is_same_v<ResponseT, game::network::DetailMapResponse>)
                {
                    if (typedResponse.requestId != m_detailRequest.requestId ||
                        typedResponse.target != m_requestedDetailTarget)
                        return;
                    m_detailMetadata = typedResponse.metadata;
                    m_detailSnapshot = std::move(typedResponse.snapshot);
                    m_detailSnapshotTarget = typedResponse.target;
                    m_hasDetail = true;
                    complete(m_detailRequest);
                }
                else if constexpr (std::is_same_v<ResponseT, game::network::HubMapResponse>)
                {
                    if (typedResponse.requestId != m_hubRequest.requestId ||
                        typedResponse.systemId != m_requestedHubSystemId ||
                        typedResponse.hubId != m_requestedHubId)
                        return;
                    m_hubMetadata = typedResponse.metadata;
                    m_hubSnapshot = std::move(typedResponse.snapshot);
                    m_hubSnapshotSystemId = typedResponse.systemId;
                    m_hubSnapshotId = std::move(typedResponse.hubId);
                    m_hasHub = true;
                    complete(m_hubRequest);
                }
            },
            std::move(response));
    }
}

bool ClientMapService::requestGalaxy(bool forceRefresh)
{
    pumpResponses();
    if (!forceRefresh && m_hasGalaxy)
        return true;
    if (m_galaxyRequest.status == ClientRequestStatus::Pending)
        return false;
    if (!forceRefresh &&
        (m_galaxyRequest.status == ClientRequestStatus::TimedOut ||
         m_galaxyRequest.status == ClientRequestStatus::Failed))
        return false;
    m_galaxyRequest.attempts = 0;
    sendGalaxyRequest();
    return false;
}

bool ClientMapService::requestSystem(int systemId, bool forceRefresh)
{
    pumpResponses();
    if (!forceRefresh && m_hasSystem && m_systemSnapshotId == systemId)
        return true;

    if (m_systemRequest.status == ClientRequestStatus::Pending)
    {
        if (m_requestedSystemId == systemId)
            return false;
        cancel(m_systemRequest);
    }
    if (!forceRefresh && m_requestedSystemId == systemId &&
        (m_systemRequest.status == ClientRequestStatus::TimedOut ||
         m_systemRequest.status == ClientRequestStatus::Failed))
        return false;

    m_requestedSystemId = systemId;
    m_systemRequest.attempts = 0;
    sendSystemRequest();
    return false;
}

bool ClientMapService::requestDetail(
    const world::celestial::DetailTarget& target,
    bool forceRefresh)
{
    if (!target.valid())
        return false;
    pumpResponses();
    if (!forceRefresh && m_hasDetail && m_detailSnapshotTarget == target)
        return true;

    if (m_detailRequest.status == ClientRequestStatus::Pending)
    {
        if (m_requestedDetailTarget == target)
            return false;
        cancel(m_detailRequest);
    }
    if (!forceRefresh && m_requestedDetailTarget == target &&
        (m_detailRequest.status == ClientRequestStatus::TimedOut ||
         m_detailRequest.status == ClientRequestStatus::Failed))
        return false;

    m_requestedDetailTarget = target;
    m_detailRequest.attempts = 0;
    sendDetailRequest();
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
    if (!forceRefresh && m_hasHub &&
        m_hubSnapshotSystemId == systemId && m_hubSnapshotId == hubId)
        return true;

    if (m_hubRequest.status == ClientRequestStatus::Pending)
    {
        if (m_requestedHubSystemId == systemId && m_requestedHubId == hubId)
            return false;
        cancel(m_hubRequest);
    }
    if (!forceRefresh &&
        m_requestedHubSystemId == systemId && m_requestedHubId == hubId &&
        (m_hubRequest.status == ClientRequestStatus::TimedOut ||
         m_hubRequest.status == ClientRequestStatus::Failed))
        return false;

    m_requestedHubSystemId = systemId;
    m_requestedHubId = hubId;
    m_hubRequest.attempts = 0;
    sendHubRequest();
    return false;
}

ClientRequestStatus ClientMapService::galaxyStatus() const { return m_galaxyRequest.status; }
ClientRequestStatus ClientMapService::systemStatus() const { return m_systemRequest.status; }
ClientRequestStatus ClientMapService::detailStatus() const { return m_detailRequest.status; }
ClientRequestStatus ClientMapService::hubStatus() const { return m_hubRequest.status; }

const game::network::SnapshotMetadata& ClientMapService::galaxyMetadata() const { return m_galaxyMetadata; }
const game::network::SnapshotMetadata& ClientMapService::systemMetadata() const { return m_systemMetadata; }
const game::network::SnapshotMetadata& ClientMapService::detailMetadata() const { return m_detailMetadata; }
const game::network::SnapshotMetadata& ClientMapService::hubMetadata() const { return m_hubMetadata; }

const world::celestial::GalaxyMapSnapshot* ClientMapService::galaxy() const { return m_hasGalaxy ? &m_galaxySnapshot : nullptr; }
const world::celestial::SystemMapSnapshot* ClientMapService::system(int systemId) const { return m_hasSystem && m_systemSnapshotId == systemId ? &m_systemSnapshot : nullptr; }
const world::celestial::DetailMapSnapshot* ClientMapService::detail(const world::celestial::DetailTarget& target) const { return m_hasDetail && m_detailSnapshotTarget == target ? &m_detailSnapshot : nullptr; }
const world::celestial::HubMapSnapshot* ClientMapService::hub(int systemId, const std::string& hubId) const
{
    return m_hasHub && m_hubSnapshotSystemId == systemId && m_hubSnapshotId == hubId ? &m_hubSnapshot : nullptr;
}
}
