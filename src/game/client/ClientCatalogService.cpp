#include "src/game/client/ClientCatalogService.h"

#include <type_traits>
#include <utility>

namespace game::client
{
ClientCatalogService::ClientCatalogService(ITransport& transport)
    : m_transport(transport)
{
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

                if constexpr (std::is_same_v<
                                  ResponseT,
                                  game::network::StarAtlasResponse>)
                {
                    if (typedResponse.requestId != m_starAtlasRequestId)
                        return;

                    if (m_hasStarAtlas &&
                        typedResponse.metadata.catalogRevision <
                            m_starAtlasMetadata.catalogRevision)
                    {
                        return;
                    }

                    m_starAtlasMetadata = typedResponse.metadata;
                    m_starAtlas = std::move(typedResponse.atlas);
                    m_hasStarAtlas = true;
                    m_starAtlasRequestId = 0;
                    m_starAtlasResponseReady = true;
                }
                else if constexpr (std::is_same_v<
                                       ResponseT,
                                       game::network::CelestialSnapshotResponse>)
                {
                    if (typedResponse.requestId != m_celestialRequestId)
                        return;

                    if (m_hasCelestialSnapshot &&
                        typedResponse.metadata.serverTick <
                            m_celestialSnapshotMetadata.serverTick)
                    {
                        return;
                    }

                    m_celestialSnapshotMetadata = typedResponse.metadata;
                    m_celestialSnapshot = std::move(typedResponse.snapshot);
                    m_hasCelestialSnapshot = true;
                    m_celestialRequestId = 0;
                    m_celestialResponseReady = true;
                }
            },
            std::move(response)
        );
    }
}

bool ClientCatalogService::requestStarAtlas()
{
    pumpResponses();

    if (m_starAtlasResponseReady)
    {
        m_starAtlasResponseReady = false;
        return true;
    }

    if (m_hasStarAtlas)
        return true;

    if (m_starAtlasRequestId != 0)
        return false;

    game::network::StarAtlasRequest request;
    request.requestId = m_nextRequestId++;
    m_starAtlasRequestId = request.requestId;
    m_transport.sendPresentationDataRequest(request);
    return false;
}

bool ClientCatalogService::requestCelestialSnapshot()
{
    pumpResponses();

    if (m_celestialResponseReady)
    {
        m_celestialResponseReady = false;
        return true;
    }

    if (m_celestialRequestId != 0)
        return false;

    game::network::CelestialSnapshotRequest request;
    request.requestId = m_nextRequestId++;
    m_celestialRequestId = request.requestId;
    m_transport.sendPresentationDataRequest(request);
    return false;
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
    return m_hasCelestialSnapshot ? &m_celestialSnapshot : nullptr;
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
