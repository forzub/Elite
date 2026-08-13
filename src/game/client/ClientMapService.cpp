#include "src/game/client/ClientMapService.h"
#include "src/game/client/ClientCatalogService.h"
#include "src/game/client/ClientCelestialMapBridge.h"
#include "src/game/client/ClientGalaxyMapBridge.h"
#include "src/game/client/ClientSystemMapShipBridge.h"
#include "src/game/client/ClientSystemMapInfrastructureBridge.h"
#include "src/game/client/ClientDetailMapBridge.h"
#include "src/game/client/ClientHubMapBridge.h"
#include "src/game/client/ClientWorldState.h"

#include <algorithm>
#include <type_traits>
#include <utility>

namespace game::client
{
ClientMapService::ClientMapService(
    ITransport& transport,
    const ClientCatalogService& catalogs,
    const ::ClientWorldState& world
)
    : m_transport(transport)
    , m_catalogs(catalogs)
    , m_world(world)
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

void ClientMapService::fail(RequestState& state)
{
    state.status = ClientRequestStatus::Failed;
    state.requestId = 0;
    state.elapsedSeconds = 0.0f;
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
    m_deferredSystemResponse.reset();

    game::network::SystemMapRequest request;
    request.requestId = nextRequestId();
    request.systemId = m_requestedSystemId;
    begin(m_systemRequest, request.requestId);
    m_transport.sendMapRequest(request);
}

void ClientMapService::sendDetailRequest()
{
    m_deferredDetailResponse.reset();

    game::network::DetailMapRequest request;
    request.requestId = nextRequestId();
    request.target = m_requestedDetailTarget;
    begin(m_detailRequest, request.requestId);
    m_transport.sendMapRequest(request);
}

void ClientMapService::sendHubRequest()
{
    m_deferredHubResponse.reset();

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

    if (m_deferredSystemResponse)
    {
        // The map reply is already in hand; only the matching replication
        // bracket is missing. Bound this wait, but do not resend an otherwise
        // valid map response and create another competing epoch.
        m_systemRequest.elapsedSeconds += std::max(dt, 0.0f);
        if (m_systemRequest.elapsedSeconds >= RequestTimeoutSeconds)
        {
            m_deferredSystemResponse.reset();
            fail(m_systemRequest);
        }
    }
    else if (advanceTimeout(m_systemRequest, dt))
    {
        sendSystemRequest();
    }

    if (m_deferredDetailResponse)
    {
        m_detailRequest.elapsedSeconds += std::max(dt, 0.0f);
        if (m_detailRequest.elapsedSeconds >= RequestTimeoutSeconds)
        {
            m_deferredDetailResponse.reset();
            fail(m_detailRequest);
        }
    }
    else if (advanceTimeout(m_detailRequest, dt))
    {
        sendDetailRequest();
    }

    if (m_deferredHubResponse)
    {
        m_hubRequest.elapsedSeconds += std::max(dt, 0.0f);
        if (m_hubRequest.elapsedSeconds >= RequestTimeoutSeconds)
        {
            m_deferredHubResponse.reset();
            fail(m_hubRequest);
        }
    }
    else if (advanceTimeout(m_hubRequest, dt))
    {
        sendHubRequest();
    }
}

void ClientMapService::resetPendingRequests()
{
    cancel(m_galaxyRequest);
    cancel(m_systemRequest);
    m_deferredSystemResponse.reset();
    cancel(m_detailRequest);
    m_deferredDetailResponse.reset();
    cancel(m_hubRequest);
    m_deferredHubResponse.reset();
}

bool ClientMapService::acceptsTimeline(
    const game::network::SnapshotMetadata& metadata
) const
{
    return
        m_universeTimelineRevision == 0 ||
        metadata.universeTimelineRevision ==
            m_universeTimelineRevision;
}

void ClientMapService::setUniverseTimelineRevision(
    std::uint64_t revision
)
{
    if (revision == 0 ||
        revision == m_universeTimelineRevision)
    {
        return;
    }

    m_universeTimelineRevision = revision;

    /*
        A universe-time discontinuity invalidates every dynamic map result.
        serverTick/serverTimeSeconds remain monotonic, so stale responses from
        the previous branch must not be allowed to repopulate the cache.
    */
    resetPendingRequests();

    m_hasGalaxy = false;
    m_hasSystem = false;
    m_hasDetail = false;
    m_hasHub = false;

    m_systemSnapshotId = -1;
    m_hubSnapshotSystemId = -1;
    m_hubSnapshotId.clear();
    m_detailSnapshotTarget = {};

    m_galaxyMetadata = {};
    m_systemMetadata = {};
    m_deferredSystemResponse.reset();
    m_deferredDetailResponse.reset();
    m_deferredHubResponse.reset();
    m_detailMetadata = {};
    m_hubMetadata = {};
}

ClientMapService::SystemResponseResult
ClientMapService::tryCompleteSystemResponse(
    game::network::SystemMapResponse& response
)
{
    const auto shipSample =
        m_world.sampleSystemMapShipsAtServerTime(
            response.systemId,
            response.metadata.serverTimeSeconds
        );

    const auto infrastructureSample =
        m_world.sampleSystemMapInfrastructureAtServerTime(
            response.systemId,
            response.metadata.serverTimeSeconds
        );

    if (shipSample.status ==
            SystemMapShipSampleStatus::AwaitingNewerSnapshot ||
        infrastructureSample.status ==
            SystemMapInfrastructureSampleStatus::AwaitingNewerSnapshot)
    {
        return SystemResponseResult::AwaitingSimulationHistory;
    }

    if (shipSample.status == SystemMapShipSampleStatus::TooOld ||
        infrastructureSample.status ==
            SystemMapInfrastructureSampleStatus::TooOld)
    {
        // The map response itself may have been delayed longer than the
        // retained replication history. Request a fresh epoch instead of
        // clamping dynamic world state to an unrelated time.
        return SystemResponseResult::RetryFreshResponse;
    }

    auto rebuiltSnapshot = response.snapshot;

    const auto* atlas = m_catalogs.starAtlas();
    const auto* celestial =
        m_catalogs.resolveCelestialSystem(
            response.systemId,
            rebuiltSnapshot.universeTimeSeconds
        );

    if (!atlas ||
        !celestial ||
        !rebuildSystemMapCelestialLayer(
            rebuiltSnapshot,
            *atlas,
            *celestial
        ))
    {
        return SystemResponseResult::Failed;
    }

    rebuildSystemMapInfrastructureLayer(
        rebuiltSnapshot,
        infrastructureSample
    );

    rebuildSystemMapShipLayer(
        rebuiltSnapshot,
        shipSample.ships,
        m_world.localControlledEntityId()
    );

    m_systemMetadata = response.metadata;
    m_systemSnapshot = std::move(rebuiltSnapshot);
    m_systemSnapshotId = response.systemId;
    m_hasSystem = true;
    complete(m_systemRequest);
    return SystemResponseResult::Ready;
}

void ClientMapService::retrySystemRequestOrFail()
{
    m_deferredSystemResponse.reset();

    if (m_systemRequest.attempts >= MaxRequestAttempts)
    {
        fail(m_systemRequest);
        return;
    }

    sendSystemRequest();
}

ClientMapService::DetailResponseResult
ClientMapService::tryCompleteDetailResponse(
    game::network::DetailMapResponse& response
)
{
    const auto runtimeSample =
        m_world.sampleDetailMapRuntimeAtServerTime(
            response.target.systemId,
            response.metadata.serverTimeSeconds
        );

    if (runtimeSample.status ==
        DetailMapRuntimeSampleStatus::AwaitingNewerSnapshot)
    {
        return DetailResponseResult::AwaitingSimulationHistory;
    }

    if (runtimeSample.status == DetailMapRuntimeSampleStatus::TooOld)
        return DetailResponseResult::RetryFreshResponse;

    const auto* atlas = m_catalogs.starAtlas();
    const auto* celestial = m_catalogs.resolveCelestialSystem(
        response.target.systemId,
        response.metadata.universeTimeSeconds
    );

    if (!atlas || !celestial)
        return DetailResponseResult::Failed;

    world::celestial::DetailMapSnapshot rebuiltSnapshot;
    if (!rebuildDetailMapFromClientState(
            rebuiltSnapshot,
            response.target,
            *atlas,
            *celestial,
            runtimeSample,
            response.metadata.serverTimeSeconds,
            response.metadata.universeTimeSeconds,
            m_world.localControlledEntityId()))
    {
        return DetailResponseResult::Failed;
    }

    m_detailMetadata = response.metadata;
    m_detailSnapshot = std::move(rebuiltSnapshot);
    m_detailSnapshotTarget = response.target;
    m_hasDetail = true;
    complete(m_detailRequest);
    return DetailResponseResult::Ready;
}

void ClientMapService::retryDetailRequestOrFail()
{
    m_deferredDetailResponse.reset();

    if (m_detailRequest.attempts >= MaxRequestAttempts)
    {
        fail(m_detailRequest);
        return;
    }

    sendDetailRequest();
}

ClientMapService::HubResponseResult
ClientMapService::tryCompleteHubResponse(
    game::network::HubMapResponse& response
)
{
    const auto runtimeSample =
        m_world.sampleHubMapRuntimeAtServerTime(
            response.systemId,
            response.metadata.serverTimeSeconds
        );

    if (runtimeSample.status ==
        DetailMapRuntimeSampleStatus::AwaitingNewerSnapshot)
    {
        return HubResponseResult::AwaitingSimulationHistory;
    }

    if (runtimeSample.status == DetailMapRuntimeSampleStatus::TooOld)
        return HubResponseResult::RetryFreshResponse;

    const auto* atlas = m_catalogs.starAtlas();
    const auto* celestial = m_catalogs.resolveCelestialSystem(
        response.systemId,
        response.metadata.universeTimeSeconds
    );

    if (!atlas || !celestial)
        return HubResponseResult::Failed;

    world::celestial::HubMapSnapshot rebuiltSnapshot;
    if (!rebuildHubMapFromClientState(
            rebuiltSnapshot,
            response.systemId,
            response.hubId,
            *atlas,
            *celestial,
            runtimeSample,
            response.metadata.serverTimeSeconds,
            response.metadata.universeTimeSeconds,
            m_world.localControlledEntityId()))
    {
        return HubResponseResult::Failed;
    }

    m_hubMetadata = response.metadata;
    m_hubSnapshot = std::move(rebuiltSnapshot);
    m_hubSnapshotSystemId = response.systemId;
    m_hubSnapshotId = response.hubId;
    m_hasHub = true;
    complete(m_hubRequest);
    return HubResponseResult::Ready;
}

void ClientMapService::retryHubRequestOrFail()
{
    m_deferredHubResponse.reset();

    if (m_hubRequest.attempts >= MaxRequestAttempts)
    {
        fail(m_hubRequest);
        return;
    }

    sendHubRequest();
}

void ClientMapService::pumpResponses()
{
    if (m_deferredSystemResponse)
    {
        const auto result =
            tryCompleteSystemResponse(*m_deferredSystemResponse);

        if (result == SystemResponseResult::Ready)
        {
            m_deferredSystemResponse.reset();
        }
        else if (result == SystemResponseResult::RetryFreshResponse)
        {
            retrySystemRequestOrFail();
        }
        else if (result == SystemResponseResult::Failed)
        {
            m_deferredSystemResponse.reset();
            fail(m_systemRequest);
        }
    }

    if (m_deferredDetailResponse)
    {
        const auto result =
            tryCompleteDetailResponse(*m_deferredDetailResponse);

        if (result == DetailResponseResult::Ready)
        {
            m_deferredDetailResponse.reset();
        }
        else if (result == DetailResponseResult::RetryFreshResponse)
        {
            retryDetailRequestOrFail();
        }
        else if (result == DetailResponseResult::Failed)
        {
            m_deferredDetailResponse.reset();
            fail(m_detailRequest);
        }
    }

    if (m_deferredHubResponse)
    {
        const auto result =
            tryCompleteHubResponse(*m_deferredHubResponse);

        if (result == HubResponseResult::Ready)
        {
            m_deferredHubResponse.reset();
        }
        else if (result == HubResponseResult::RetryFreshResponse)
        {
            retryHubRequestOrFail();
        }
        else if (result == HubResponseResult::Failed)
        {
            m_deferredHubResponse.reset();
            fail(m_hubRequest);
        }
    }

    game::network::MapResponse response;

    while (m_transport.receiveMapResponse(response))
    {
        std::visit(
            [this](auto&& typedResponse)
            {
                using ResponseT = std::decay_t<decltype(typedResponse)>;

                if (!acceptsTimeline(typedResponse.metadata))
                    return;

                if constexpr (std::is_same_v<ResponseT, game::network::GalaxyMapResponse>)
                {
                    if (typedResponse.requestId != m_galaxyRequest.requestId)
                        return;

                    const auto* atlas = m_catalogs.starAtlas();
                    if (!atlas)
                    {
                        fail(m_galaxyRequest);
                        return;
                    }

                    auto rebuiltSnapshot = std::move(typedResponse.snapshot);
                    rebuildGalaxyMapCatalogLayer(rebuiltSnapshot, *atlas);

                    m_galaxyMetadata = typedResponse.metadata;
                    m_galaxySnapshot = std::move(rebuiltSnapshot);
                    m_hasGalaxy = true;
                    complete(m_galaxyRequest);
                }
                else if constexpr (std::is_same_v<ResponseT, game::network::SystemMapResponse>)
                {
                    if (typedResponse.requestId != m_systemRequest.requestId ||
                        typedResponse.systemId != m_requestedSystemId)
                    {
                        return;
                    }

                    const auto result =
                        tryCompleteSystemResponse(typedResponse);

                    if (result ==
                        SystemResponseResult::AwaitingSimulationHistory)
                    {
                        // A map request may be answered on a fixed server tick
                        // between normal replication publications. Preserve the
                        // response until snapshot history brackets that exact
                        // server-time epoch instead of mixing two map epochs.
                        m_deferredSystemResponse =
                            std::move(typedResponse);
                        // Start a fresh bounded wait for the replication
                        // bracket; map transport latency has already ended.
                        m_systemRequest.elapsedSeconds = 0.0f;
                    }
                    else if (result ==
                             SystemResponseResult::RetryFreshResponse)
                    {
                        retrySystemRequestOrFail();
                    }
                    else if (result == SystemResponseResult::Failed)
                    {
                        fail(m_systemRequest);
                    }
                }
                else if constexpr (std::is_same_v<ResponseT, game::network::DetailMapResponse>)
                {
                    if (typedResponse.requestId != m_detailRequest.requestId ||
                        typedResponse.target != m_requestedDetailTarget)
                    {
                        return;
                    }

                    const auto result =
                        tryCompleteDetailResponse(typedResponse);

                    if (result ==
                        DetailResponseResult::AwaitingSimulationHistory)
                    {
                        m_deferredDetailResponse = std::move(typedResponse);
                        m_detailRequest.elapsedSeconds = 0.0f;
                    }
                    else if (result ==
                             DetailResponseResult::RetryFreshResponse)
                    {
                        retryDetailRequestOrFail();
                    }
                    else if (result == DetailResponseResult::Failed)
                    {
                        fail(m_detailRequest);
                    }
                }
                else if constexpr (std::is_same_v<ResponseT, game::network::HubMapResponse>)
                {
                    if (typedResponse.requestId != m_hubRequest.requestId ||
                        typedResponse.systemId != m_requestedHubSystemId ||
                        typedResponse.hubId != m_requestedHubId)
                    {
                        return;
                    }

                    const auto result = tryCompleteHubResponse(typedResponse);
                    if (result == HubResponseResult::AwaitingSimulationHistory)
                    {
                        m_deferredHubResponse = std::move(typedResponse);
                        m_hubRequest.elapsedSeconds = 0.0f;
                    }
                    else if (result == HubResponseResult::RetryFreshResponse)
                    {
                        retryHubRequestOrFail();
                    }
                    else if (result == HubResponseResult::Failed)
                    {
                        fail(m_hubRequest);
                    }
                }
            },
            std::move(response));
    }
}

bool ClientMapService::requestGalaxy(bool forceRefresh)
{
    pumpResponses();

    // Galaxy catalog geometry is deterministic client-owned data. A server
    // reply only supplies authoritative overlays/epoch, so do not request one
    // until the local StarAtlas needed to complete it is available.
    if (!m_catalogs.hasStarAtlas())
        return false;

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

    // Static celestial definitions live in the client's local catalog. Do not
    // ask the server for a System-map dynamic layer until that dependency is
    // available locally; otherwise a response cannot form one epoch-coherent
    // map snapshot.
    if (!m_catalogs.hasStarAtlas())
        return false;

    if (!forceRefresh && m_hasSystem && m_systemSnapshotId == systemId)
        return true;

    if (m_systemRequest.status == ClientRequestStatus::Pending)
    {
        if (m_requestedSystemId == systemId)
            return false;
        cancel(m_systemRequest);
        m_deferredSystemResponse.reset();
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
    if (!target.valid() || !m_catalogs.hasStarAtlas())
        return false;
    pumpResponses();
    if (!forceRefresh && m_hasDetail && m_detailSnapshotTarget == target)
        return true;

    if (m_detailRequest.status == ClientRequestStatus::Pending)
    {
        if (m_requestedDetailTarget == target)
            return false;
        cancel(m_detailRequest);
        m_deferredDetailResponse.reset();
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
    if (systemId < 0 || hubId.empty() || !m_catalogs.hasStarAtlas())
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
        m_deferredHubResponse.reset();
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
