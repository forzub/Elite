#include "src/game/client/ClientCatalogService.h"

#include <algorithm>
#include <type_traits>
#include <utility>

namespace game::client
{
ClientCatalogService::ClientCatalogService(ITransport& transport)
    : m_transport(transport)
{
}

std::uint64_t ClientCatalogService::nextRequestId()
{
    return m_nextRequestId++;
}

void ClientCatalogService::begin(
    RequestState& state,
    std::uint64_t requestId
)
{
    state.status = ClientRequestStatus::Pending;
    state.requestId = requestId;
    state.elapsedSeconds = 0.0f;
    ++state.attempts;
}

void ClientCatalogService::complete(RequestState& state)
{
    state.status = ClientRequestStatus::Ready;
    state.requestId = 0;
    state.elapsedSeconds = 0.0f;
    state.attempts = 0;
}

void ClientCatalogService::cancel(RequestState& state)
{
    if (state.status == ClientRequestStatus::Pending)
        state.status = ClientRequestStatus::Cancelled;

    state.requestId = 0;
    state.elapsedSeconds = 0.0f;
    state.attempts = 0;
}

bool ClientCatalogService::advanceTimeout(
    RequestState& state,
    float dt
)
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

void ClientCatalogService::sendStarAtlasRequest()
{
    game::network::StarAtlasRequest request;
    request.requestId = nextRequestId();
    begin(m_starAtlasRequest, request.requestId);
    m_transport.sendPresentationDataRequest(request);
}

void ClientCatalogService::update(float dt)
{
    pumpResponses();

    if (advanceTimeout(m_starAtlasRequest, dt))
        sendStarAtlasRequest();
}

void ClientCatalogService::resetPendingRequests()
{
    cancel(m_starAtlasRequest);
    m_hasCelestialSnapshot = false;
    m_celestialSnapshot = {};
    m_celestialSnapshotMetadata = {};
}

void ClientCatalogService::pumpResponses()
{
    game::network::PresentationDataResponse response;

    while (m_transport.receivePresentationDataResponse(response))
    {
        std::visit(
            [this](auto&& typedResponse)
            {
                using ResponseT =
                    std::decay_t<decltype(typedResponse)>;

                if constexpr (std::is_same_v<
                                  ResponseT,
                                  game::network::StarAtlasResponse>)
                {
                    if (typedResponse.requestId !=
                        m_starAtlasRequest.requestId)
                    {
                        return;
                    }

                    if (m_hasStarAtlas &&
                        typedResponse.metadata.catalogRevision <
                            m_starAtlasMetadata.catalogRevision)
                    {
                        return;
                    }

                    m_starAtlasMetadata = typedResponse.metadata;
                    m_starAtlas = std::move(typedResponse.atlas);
                    m_celestialRuntimes.initialize(m_starAtlas);
                    m_hasStarAtlas = true;
                    m_hasCelestialSnapshot = false;
                    complete(m_starAtlasRequest);
                }
            },
            std::move(response)
        );
    }
}

bool ClientCatalogService::requestStarAtlas()
{
    pumpResponses();

    if (m_hasStarAtlas)
        return true;

    if (m_starAtlasRequest.status == ClientRequestStatus::Pending ||
        m_starAtlasRequest.status == ClientRequestStatus::TimedOut ||
        m_starAtlasRequest.status == ClientRequestStatus::Failed)
    {
        return false;
    }

    m_starAtlasRequest.attempts = 0;
    sendStarAtlasRequest();
    return false;
}

bool ClientCatalogService::resolveCelestialSnapshot(
    int systemId,
    double universeTimeSeconds,
    const game::network::SnapshotMetadata& sourceMetadata,
    bool forceRefresh
)
{
    pumpResponses();

    if (!m_hasStarAtlas)
        return false;

    if (!forceRefresh &&
        m_hasCelestialSnapshot &&
        m_celestialSnapshot.systemId == systemId &&
        m_celestialSnapshot.simTimeSeconds == universeTimeSeconds)
    {
        return true;
    }

    const world::celestial::CelestialSystemSnapshot* resolved =
        m_celestialRuntimes.resolve(
            systemId,
            universeTimeSeconds
        );

    if (!resolved)
        return false;

    m_celestialSnapshot = *resolved;
    m_celestialSnapshotMetadata = sourceMetadata;
    m_celestialSnapshotMetadata.universeTimeSeconds =
        universeTimeSeconds;
    m_hasCelestialSnapshot = true;
    return true;
}

ClientRequestStatus ClientCatalogService::starAtlasStatus() const
{
    return m_starAtlasRequest.status;
}

ClientRequestStatus ClientCatalogService::celestialStatus() const
{
    if (m_hasCelestialSnapshot)
        return ClientRequestStatus::Ready;

    if (m_starAtlasRequest.status == ClientRequestStatus::TimedOut)
        return ClientRequestStatus::TimedOut;

    if (m_starAtlasRequest.status == ClientRequestStatus::Failed)
        return ClientRequestStatus::Failed;

    return m_hasStarAtlas
        ? ClientRequestStatus::Idle
        : m_starAtlasRequest.status;
}

bool ClientCatalogService::hasStarAtlas() const
{
    return m_hasStarAtlas;
}

bool ClientCatalogService::hasCelestialSnapshot() const
{
    return m_hasCelestialSnapshot;
}

const world::celestial::StarAtlasDatabase*
ClientCatalogService::starAtlas() const
{
    return m_hasStarAtlas ? &m_starAtlas : nullptr;
}

const world::celestial::CelestialSystemSnapshot*
ClientCatalogService::celestialSnapshot() const
{
    return m_hasCelestialSnapshot
        ? &m_celestialSnapshot
        : nullptr;
}

const world::celestial::CelestialSystemSnapshot*
ClientCatalogService::resolveCelestialSystem(
    int systemId,
    double universeTimeSeconds
) const
{
    if (!m_hasStarAtlas)
        return nullptr;

    return m_celestialRuntimes.resolve(
        systemId,
        universeTimeSeconds
    );
}

const game::network::CatalogMetadata&
ClientCatalogService::starAtlasMetadata() const
{
    return m_starAtlasMetadata;
}

const game::network::SnapshotMetadata&
ClientCatalogService::celestialMetadata() const
{
    return m_celestialSnapshotMetadata;
}
}
