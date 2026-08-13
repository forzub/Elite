#include "src/game/client/ClientCatalogService.h"

#include <sstream>

namespace game::client
{
bool ClientCatalogService::loadLocalStarAtlas()
{
    if (m_hasStarAtlas)
        return true;

    if (!m_starAtlas.loadFromRuntimeOrSource())
        return false;

    m_celestialRuntimes.initialize(m_starAtlas);
    m_hasStarAtlas = true;
    m_hasCelestialSnapshot = false;
    return true;
}

void ClientCatalogService::resetRuntimeState()
{
    m_hasCelestialSnapshot = false;
    m_celestialSnapshot = {};
    m_celestialSnapshotMetadata = {};
}

bool ClientCatalogService::validateServerStarAtlas(
    const game::network::CatalogMetadata& serverMetadata,
    std::string* errorMessage
) const
{
    const auto local = localStarAtlasMetadata();

    if (!m_hasStarAtlas)
    {
        if (errorMessage)
            *errorMessage = "local StarAtlas is not loaded";
        return false;
    }

    if (serverMetadata.schemaVersion != local.schemaVersion)
    {
        if (errorMessage)
        {
            std::ostringstream out;
            out << "StarAtlas schema mismatch: server="
                << serverMetadata.schemaVersion
                << " client=" << local.schemaVersion;
            *errorMessage = out.str();
        }
        return false;
    }

    if (serverMetadata.contentFingerprint != local.contentFingerprint)
    {
        if (errorMessage)
        {
            std::ostringstream out;
            out << "StarAtlas content mismatch: server=0x"
                << std::hex << serverMetadata.contentFingerprint
                << " client=0x" << local.contentFingerprint;
            *errorMessage = out.str();
        }
        return false;
    }

    if (errorMessage)
        errorMessage->clear();

    return true;
}

bool ClientCatalogService::resolveCelestialSnapshot(
    int systemId,
    double universeTimeSeconds,
    const game::network::SnapshotMetadata& sourceMetadata,
    bool forceRefresh
)
{
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

game::network::CatalogMetadata
ClientCatalogService::localStarAtlasMetadata() const
{
    game::network::CatalogMetadata metadata;
    metadata.schemaVersion =
        world::celestial::StarAtlasDatabase::CatalogSchemaVersion;
    metadata.contentFingerprint =
        m_hasStarAtlas ? m_starAtlas.contentFingerprint() : 0;
    return metadata;
}

const game::network::SnapshotMetadata&
ClientCatalogService::celestialMetadata() const
{
    return m_celestialSnapshotMetadata;
}
}
