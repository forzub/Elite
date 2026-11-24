#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "src/world/celestial/CelestialTypes.h"
#include "src/game/navigation/OwnedNavigationAsset.h"
#include "src/game/simulation/ClientNavigationSensorSnapshot.h"
#include "src/game/simulation/ClientWalletSnapshot.h"
#include "src/game/simulation/ClientTradeSnapshot.h"
#include "src/world/WorldParams.h"

namespace game::simulation
{

// a construction contract's project override in force (elite::model_asset::HubExpansionOverride; audit SRV-1): the
// client applies it so its city geometry is the server's
struct HubConstructionHaltSnapshot
{
    int from = 0;
    int to = -1;
};
struct HubConstructionOverrideSnapshot
{
    std::string hubId;
    std::string projectId;
    int startDay = 0;
    int workDays = 1;
    std::vector<HubConstructionHaltSnapshot> halts;
};

struct ClientSessionSnapshot
{
    world::celestial::PlayerNavigationState playerNavigation;
    std::vector<game::navigation::OwnedNavigationAsset> ownedNavigationAssets;
    game::simulation::ClientNavigationSensorSnapshot navigationSensors;
    game::simulation::ClientWalletSnapshot wallet;   // per session (GameServer::walletForSession)
    game::simulation::ClientTradeSnapshot trade;     // per session (GameServer::tradeForSession)
    WorldParams predictionWorldParams {0.0f, 50.0f};
    double universeTimeSeconds = 0.0;
    double universeTimeScale = 1.0;
    std::uint64_t universeTimelineRevision = 1;
    double configuredUniverseTimeScale = 10000.0;

    bool universeTimeSimulation = false;
    std::string universeDate;
    // the ship this session flies and its control epoch (base 2: a ship change mid-session; 0 = the bootstrap ship)
    std::uint32_t controlledEntityId = 0;
    std::uint32_t controlEpoch = 0;
    // every construction override in force on the server (usually none; a few with players' contracts)
    std::vector<HubConstructionOverrideSnapshot> constructionOverrides;
};

} // namespace game::simulation
