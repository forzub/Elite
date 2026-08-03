#pragma once

#include <cstdint>
#include <string>

#include "src/game/network/ITransport.h"
#include "src/game/network/ProtocolMetadata.h"
#include "src/world/celestial/CelestialTypes.h"

namespace game::client
{
class ClientMapService
{
public:
    explicit ClientMapService(ITransport& transport);

    void pumpResponses();

    bool requestGalaxy(bool forceRefresh = false);
    bool requestSystem(int systemId, bool forceRefresh = false);
    bool requestDetail(
        const world::celestial::DetailTarget& target,
        bool forceRefresh = false);
    bool requestHub(
        int systemId,
        const std::string& hubId,
        bool forceRefresh = false);

    const game::network::SnapshotMetadata& galaxyMetadata() const;
    const game::network::SnapshotMetadata& systemMetadata() const;
    const game::network::SnapshotMetadata& detailMetadata() const;
    const game::network::SnapshotMetadata& hubMetadata() const;

    const world::celestial::GalaxyMapSnapshot* galaxy() const;
    const world::celestial::SystemMapSnapshot* system(int systemId) const;
    const world::celestial::DetailMapSnapshot* detail(
        const world::celestial::DetailTarget& target) const;
    const world::celestial::HubMapSnapshot* hub(
        int systemId,
        const std::string& hubId) const;

private:
    ITransport& m_transport;
    std::uint64_t m_nextRequestId = 1;
    std::uint64_t m_galaxyRequestId = 0;
    std::uint64_t m_systemRequestId = 0;
    std::uint64_t m_detailRequestId = 0;
    std::uint64_t m_hubRequestId = 0;

    bool m_galaxyResponseReady = false;
    bool m_systemResponseReady = false;
    bool m_detailResponseReady = false;
    bool m_hubResponseReady = false;

    int m_requestedSystemId = -1;
    world::celestial::DetailTarget m_requestedDetailTarget;
    int m_requestedHubSystemId = -1;
    std::string m_requestedHubId;

    bool m_hasGalaxy = false;
    bool m_hasSystem = false;
    bool m_hasDetail = false;
    bool m_hasHub = false;
    int m_systemSnapshotId = -1;
    int m_hubSnapshotSystemId = -1;
    std::string m_hubSnapshotId;
    world::celestial::DetailTarget m_detailSnapshotTarget;

    game::network::SnapshotMetadata m_galaxyMetadata;
    game::network::SnapshotMetadata m_systemMetadata;
    game::network::SnapshotMetadata m_detailMetadata;
    game::network::SnapshotMetadata m_hubMetadata;

    world::celestial::GalaxyMapSnapshot m_galaxySnapshot;
    world::celestial::SystemMapSnapshot m_systemSnapshot;
    world::celestial::DetailMapSnapshot m_detailSnapshot;
    world::celestial::HubMapSnapshot m_hubSnapshot;
};
}
