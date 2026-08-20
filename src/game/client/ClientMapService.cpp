#include "src/game/client/ClientMapService.h"
#include "src/game/client/ClientCatalogService.h"
#include "src/game/client/ClientCelestialMapBridge.h"
#include "src/game/client/ClientGalaxyMapBridge.h"
#include "src/game/client/ClientSystemMapShipBridge.h"
#include "src/game/client/ClientSystemMapInfrastructureBridge.h"
#include "src/game/client/ClientDetailMapBridge.h"
#include "src/game/client/ClientHubMapBridge.h"
#include "src/game/client/ClientWorldState.h"
#include "src/game/diagnostics/HubMotionLab.h"

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

void ClientMapService::update(float dt)
{
    pumpResponses();
    if (advanceTimeout(m_galaxyRequest, dt))
        sendGalaxyRequest();
}

void ClientMapService::resetPendingRequests()
{
    cancel(m_galaxyRequest);
}

bool ClientMapService::acceptsTimeline(
    const game::network::SnapshotMetadata& metadata
) const
{
    return
        m_universeTimelineRevision == 0 ||
        metadata.universeTimelineRevision == m_universeTimelineRevision;
}

void ClientMapService::setUniverseTimelineRevision(std::uint64_t revision)
{
    if (revision == 0 || revision == m_universeTimelineRevision)
        return;

    m_universeTimelineRevision = revision;
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
    m_detailMetadata = {};
    m_hubMetadata = {};
}

bool ClientMapService::composeSystem(
    int systemId,
    const game::network::SnapshotMetadata& sourceMetadata,
    double universeTimeScale,
    const std::string& universeDate
)
{
    if (systemId < 0 ||
        !m_catalogs.hasStarAtlas() ||
        !acceptsTimeline(sourceMetadata))
    {
        return false;
    }

    const auto shipSample = m_world.sampleSystemMapShipsAtServerTime(
        systemId,
        sourceMetadata.serverTimeSeconds
    );
    const auto infrastructureSample =
        m_world.sampleSystemMapInfrastructureAtServerTime(
            systemId,
            sourceMetadata.serverTimeSeconds
        );

    if (shipSample.status != SystemMapShipSampleStatus::Ready ||
        infrastructureSample.status !=
            SystemMapInfrastructureSampleStatus::Ready)
    {
        return false;
    }

    const auto* atlas = m_catalogs.starAtlas();
    const auto* celestial = m_catalogs.resolveCelestialSystem(
        systemId,
        sourceMetadata.universeTimeSeconds
    );
    if (!atlas || !celestial)
        return false;

    world::celestial::SystemMapSnapshot rebuilt;
    rebuilt.systemId = systemId;
    rebuilt.universeTimeSeconds = sourceMetadata.universeTimeSeconds;
    rebuilt.universeTimeScale = universeTimeScale;
    rebuilt.universeDate = universeDate;

    if (!rebuildSystemMapCelestialLayer(rebuilt, *atlas, *celestial))
        return false;

    rebuildSystemMapInfrastructureLayer(rebuilt, infrastructureSample);
    rebuildSystemMapShipLayer(
        rebuilt,
        shipSample.ships,
        m_world.localControlledEntityId()
    );

    // Hub Motion Lab is presentation diagnostics, not authoritative gameplay.
    // Keep its analytic System-map probe client-side now that SystemMapResponse
    // no longer exists.
    if (game::diagnostics::HubMotionLabEnabled &&
        systemId == game::diagnostics::HubMotionLabSystemId)
    {
        const auto runtime = m_world.sampleDetailMapRuntimeAtServerTime(
            systemId,
            sourceMetadata.serverTimeSeconds
        );
        if (runtime.status == DetailMapRuntimeSampleStatus::Ready)
        {
            const auto* hub = findDetailRuntimeHub(
                runtime,
                std::string(game::diagnostics::HubMotionLabHubId)
            );
            if (hub)
            {
                const auto pose = game::diagnostics::evaluateHubMotionLabCube(
                    sourceMetadata.serverTimeSeconds
                );
                world::celestial::SystemMapObject cube;
                cube.stableId = "diagnostic:hub_motion_lab_cube";
                cube.name = "LAB ANALYTIC CUBE";
                cube.parentBodyId = hub->parentBodyId;
                cube.kind = world::celestial::SystemMapObjectKind::Ship;
                const glm::dvec3 worldMeters =
                    world::coordinates::fullMeters(hub->worldPosition) +
                    detailHubLocalToWorldVector(*hub, pose.localPositionMeters);
                cube.positionAu =
                    worldMeters / world::celestial::MetersPerAu;
                cube.systemId = systemId;
                cube.hasOrbit = false;
                rebuilt.objects.push_back(std::move(cube));
            }
        }
    }

    m_systemMetadata = sourceMetadata;
    m_systemSnapshot = std::move(rebuilt);
    m_systemSnapshotId = systemId;
    m_hasSystem = true;
    return true;
}

bool ClientMapService::composeDetail(
    const world::celestial::DetailTarget& target,
    const game::network::SnapshotMetadata& sourceMetadata
)
{
    if (!target.valid() || !acceptsTimeline(sourceMetadata))
        return false;

    // Empty Galaxy sectors have no CelestialSystemSnapshot and no positive
    // system id. Their Details scene is nevertheless a complete navigation
    // address: a terminal cube inside the selected sector-local frame.
    if (target.sceneKind == world::celestial::DetailSceneKind::SpatialVolume &&
        target.systemId < 0)
    {
        world::celestial::DetailMapSnapshot rebuilt;
        if (!rebuildUnboundSpatialDetailMap(
                rebuilt,
                target,
                sourceMetadata.universeTimeSeconds))
        {
            return false;
        }

        m_detailMetadata = sourceMetadata;
        m_detailSnapshot = std::move(rebuilt);
        m_detailSnapshotTarget = target;
        m_hasDetail = true;
        return true;
    }

    if (!m_catalogs.hasStarAtlas())
        return false;

    const auto runtimeSample = m_world.sampleDetailMapRuntimeAtServerTime(
        target.systemId,
        sourceMetadata.serverTimeSeconds
    );
    if (runtimeSample.status != DetailMapRuntimeSampleStatus::Ready)
        return false;

    const auto* atlas = m_catalogs.starAtlas();
    const auto* celestial = m_catalogs.resolveCelestialSystem(
        target.systemId,
        sourceMetadata.universeTimeSeconds
    );
    if (!atlas || !celestial)
        return false;

    world::celestial::DetailMapSnapshot rebuilt;
    if (!rebuildDetailMapFromClientState(
            rebuilt,
            target,
            *atlas,
            *celestial,
            runtimeSample,
            sourceMetadata.serverTimeSeconds,
            sourceMetadata.universeTimeSeconds,
            m_world.localControlledEntityId()))
    {
        return false;
    }

    m_detailMetadata = sourceMetadata;
    m_detailSnapshot = std::move(rebuilt);
    m_detailSnapshotTarget = target;
    m_hasDetail = true;
    return true;
}

bool ClientMapService::composeHub(
    int systemId,
    const std::string& hubId,
    const game::network::SnapshotMetadata& sourceMetadata
)
{
    if (systemId < 0 ||
        hubId.empty() ||
        !m_catalogs.hasStarAtlas() ||
        !acceptsTimeline(sourceMetadata))
    {
        return false;
    }

    const auto runtimeSample = m_world.sampleHubMapRuntimeAtServerTime(
        systemId,
        sourceMetadata.serverTimeSeconds
    );
    if (runtimeSample.status != DetailMapRuntimeSampleStatus::Ready)
        return false;

    const auto* atlas = m_catalogs.starAtlas();
    const auto* celestial = m_catalogs.resolveCelestialSystem(
        systemId,
        sourceMetadata.universeTimeSeconds
    );
    if (!atlas || !celestial)
        return false;

    world::celestial::HubMapSnapshot rebuilt;
    if (!rebuildHubMapFromClientState(
            rebuilt,
            systemId,
            hubId,
            *atlas,
            *celestial,
            runtimeSample,
            sourceMetadata.serverTimeSeconds,
            sourceMetadata.universeTimeSeconds,
            m_world.localControlledEntityId()))
    {
        return false;
    }

    m_hubMetadata = sourceMetadata;
    m_hubSnapshot = std::move(rebuilt);
    m_hubSnapshotSystemId = systemId;
    m_hubSnapshotId = hubId;
    m_hasHub = true;
    return true;
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
                static_assert(
                    std::is_same_v<ResponseT, game::network::GalaxyMapResponse>,
                    "MapResponse must remain Galaxy-only until another map RPC carries unique authoritative data"
                );

                if (!acceptsTimeline(typedResponse.metadata) ||
                    typedResponse.requestId != m_galaxyRequest.requestId)
                {
                    return;
                }

                const auto* atlas = m_catalogs.starAtlas();
                if (!atlas)
                {
                    fail(m_galaxyRequest);
                    return;
                }

                auto rebuilt = std::move(typedResponse.snapshot);
                rebuildGalaxyMapCatalogLayer(rebuilt, *atlas);

                m_galaxyMetadata = typedResponse.metadata;
                m_galaxySnapshot = std::move(rebuilt);
                m_hasGalaxy = true;
                complete(m_galaxyRequest);
            },
            std::move(response)
        );
    }
}

bool ClientMapService::requestGalaxy(bool forceRefresh)
{
    pumpResponses();

    if (!m_catalogs.hasStarAtlas())
        return false;
    if (!forceRefresh && m_hasGalaxy)
        return true;
    if (m_galaxyRequest.status == ClientRequestStatus::Pending)
        return false;
    if (!forceRefresh &&
        (m_galaxyRequest.status == ClientRequestStatus::TimedOut ||
         m_galaxyRequest.status == ClientRequestStatus::Failed))
    {
        return false;
    }

    m_galaxyRequest.attempts = 0;
    sendGalaxyRequest();
    return false;
}

ClientRequestStatus ClientMapService::galaxyStatus() const
{
    return m_galaxyRequest.status;
}

const game::network::SnapshotMetadata& ClientMapService::galaxyMetadata() const
{
    return m_galaxyMetadata;
}
const game::network::SnapshotMetadata& ClientMapService::systemMetadata() const
{
    return m_systemMetadata;
}
const game::network::SnapshotMetadata& ClientMapService::detailMetadata() const
{
    return m_detailMetadata;
}
const game::network::SnapshotMetadata& ClientMapService::hubMetadata() const
{
    return m_hubMetadata;
}

const world::celestial::GalaxyMapSnapshot* ClientMapService::galaxy() const
{
    return m_hasGalaxy ? &m_galaxySnapshot : nullptr;
}
const world::celestial::SystemMapSnapshot* ClientMapService::system(int systemId) const
{
    return m_hasSystem && m_systemSnapshotId == systemId
        ? &m_systemSnapshot
        : nullptr;
}
const world::celestial::DetailMapSnapshot* ClientMapService::detail(
    const world::celestial::DetailTarget& target) const
{
    return m_hasDetail && m_detailSnapshotTarget == target
        ? &m_detailSnapshot
        : nullptr;
}
const world::celestial::HubMapSnapshot* ClientMapService::hub(
    int systemId,
    const std::string& hubId) const
{
    return m_hasHub &&
        m_hubSnapshotSystemId == systemId &&
        m_hubSnapshotId == hubId
            ? &m_hubSnapshot
            : nullptr;
}
}
