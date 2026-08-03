#pragma once

#include <string>

#include "src/game/client/ClientRequestStatus.h"
#include "src/game/network/ProtocolMetadata.h"
#include "src/world/celestial/DetailMapTypes.h"

namespace game::client
{
class MapTransitionController
{
public:
    enum class Kind
    {
        None,
        System,
        Detail,
        Hub
    };

    void beginSystem(int systemId);
    void beginDetail(const world::celestial::DetailTarget& target);
    void beginHub(int systemId, std::string hubId);
    void clear();

    bool pending() const;
    Kind kind() const;
    int systemId() const;
    const world::celestial::DetailTarget& detailTarget() const;
    const std::string& hubId() const;

    static bool requestFailed(ClientRequestStatus status);
    static bool simulationHasReached(
        const game::network::SnapshotMetadata& mapMetadata,
        const game::network::SnapshotMetadata& simulationMetadata
    );

private:
    Kind m_kind = Kind::None;
    int m_systemId = -1;
    world::celestial::DetailTarget m_detailTarget;
    std::string m_hubId;
};
}
