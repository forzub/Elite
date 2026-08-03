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

std::uint64_t ClientCatalogService::nextRequestId() { return m_nextRequestId++; }

void ClientCatalogService::begin(RequestState& state, std::uint64_t requestId)
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

bool ClientCatalogService::advanceTimeout(RequestState& state, float dt)
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

void ClientCatalogService::sendCelestialRequest()
{
    game::network::CelestialSnapshotRequest request;
    request.requestId = nextRequestId();
    begin(m_celestialRequest, request.requestId);
    m_transport.sendPresentationDataRequest(request);
}

void ClientCatalogService::update(float dt)
{
    pumpResponses();
    if (advanceTimeout(m_starAtlasRequest, dt))
        sendStarAtlasRequest();
    if (advanceTimeout(m_celestialRequest, dt))
        sendCelestialRequest();
}

void ClientCatalogService::resetPendingRequests()
{
    cancel(m_starAtlasRequest);
    cancel(m_celestialRequest);
}

void ClientCatalogService::pumpResponses()
{
    game::network::PresentationDataResponse response;
    while (m_transport.receivePresentationDataResponse(response))
    {
        std::visit(
            [this](auto&& typedResponse)
            {
                using ResponseT = std::decay_t<decltype(typedResponse)>;
                if constexpr (std::is_same_v<ResponseT, game::network::StarAtlasResponse>)
                {
                    if (typedResponse.requestId != m_starAtlasRequest.requestId)
                        return;
                    if (m_hasStarAtlas && typedResponse.metadata.catalogRevision < m_starAtlasMetadata.catalogRevision)
                        return;
                    m_starAtlasMetadata = typedResponse.metadata;
                    m_starAtlas = std::move(typedResponse.atlas);
                    m_hasStarAtlas = true;
                    complete(m_starAtlasRequest);
                }
                else if constexpr (std::is_same_v<ResponseT, game::network::CelestialSnapshotResponse>)
                {
                    if (typedResponse.requestId != m_celestialRequest.requestId)
                        return;
                    if (m_hasCelestialSnapshot && typedResponse.metadata.serverTick < m_celestialSnapshotMetadata.serverTick)
                        return;
                    m_celestialSnapshotMetadata = typedResponse.metadata;
                    m_celestialSnapshot = std::move(typedResponse.snapshot);
                    m_hasCelestialSnapshot = true;
                    complete(m_celestialRequest);
                }
            },
            std::move(response));
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
        return false;
    m_starAtlasRequest.attempts = 0;
    sendStarAtlasRequest();
    return false;
}

bool ClientCatalogService::requestCelestialSnapshot()
{
    pumpResponses();
    if (m_hasCelestialSnapshot)
        return true;
    if (m_celestialRequest.status == ClientRequestStatus::Pending ||
        m_celestialRequest.status == ClientRequestStatus::TimedOut ||
        m_celestialRequest.status == ClientRequestStatus::Failed)
        return false;
    m_celestialRequest.attempts = 0;
    sendCelestialRequest();
    return false;
}

ClientRequestStatus ClientCatalogService::starAtlasStatus() const { return m_starAtlasRequest.status; }
ClientRequestStatus ClientCatalogService::celestialStatus() const { return m_celestialRequest.status; }
bool ClientCatalogService::hasStarAtlas() const { return m_hasStarAtlas; }
bool ClientCatalogService::hasCelestialSnapshot() const { return m_hasCelestialSnapshot; }
const world::celestial::StarAtlasDatabase* ClientCatalogService::starAtlas() const { return m_hasStarAtlas ? &m_starAtlas : nullptr; }
const world::celestial::CelestialSystemSnapshot* ClientCatalogService::celestialSnapshot() const { return m_hasCelestialSnapshot ? &m_celestialSnapshot : nullptr; }
const game::network::CatalogMetadata& ClientCatalogService::starAtlasMetadata() const { return m_starAtlasMetadata; }
const game::network::SnapshotMetadata& ClientCatalogService::celestialMetadata() const { return m_celestialSnapshotMetadata; }
}
