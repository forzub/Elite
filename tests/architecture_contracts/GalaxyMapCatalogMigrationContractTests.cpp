#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "src/game/client/ClientGalaxyMapBridge.h"

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool near(double a, double b)
{
    return std::abs(a - b) < 1e-12;
}
}

int main()
{
    std::vector<world::celestial::StarSystemSummary> systems;

    world::celestial::StarSystemSummary sol;
    sol.id = 0;
    sol.name = "Sol";
    sol.starType = "G2V";
    sol.starsCount = 1;
    sol.positionLy = {1.0, 2.0, 3.0};
    systems.push_back(sol);

    world::celestial::StarSystemSummary remote;
    remote.id = 7;
    remote.name = "Remote";
    remote.starType = "K4V";
    remote.starsCount = 2;
    remote.positionLy = {11.0, 12.0, 13.0};
    systems.push_back(remote);

    std::vector<world::celestial::GalaxyObjectDefinition> objects;
    world::celestial::GalaxyObjectDefinition object;
    object.id = "relay-1";
    object.name = "Relay One";
    object.objectType = "relay";
    object.positionLy = {4.0, 5.0, 6.0};
    object.description = "Catalog object";
    object.tags = {"nav", "quest"};
    objects.push_back(object);

    world::celestial::GalaxyMapSnapshot snapshot;
    snapshot.universeTimeSeconds = 12345.0;
    snapshot.universeDate = "3026-01-01";

    // Server-owned overlay intentionally contains no static catalog fields.
    world::celestial::GalaxyMapSystem overlay;
    overlay.id = 7;
    overlay.jurisdiction = "Outer League";
    snapshot.systems.push_back(overlay);

    game::client::rebuildGalaxyMapCatalogLayer(
        snapshot,
        systems,
        objects
    );

    require(snapshot.systems.size() == 2, "local Galaxy systems were not reconstructed");
    require(snapshot.objects.size() == 1, "local Galaxy objects were not reconstructed");

    const auto& rebuiltSol = snapshot.systems[0];
    require(rebuiltSol.id == 0 && rebuiltSol.name == "Sol", "Sol identity did not come from local catalog");
    require(rebuiltSol.starType == "G2V" && rebuiltSol.starsCount == 1, "Sol static class did not come from local catalog");
    require(near(rebuiltSol.positionLy.x, 1.0), "Sol position did not come from local catalog");
    require(rebuiltSol.jurisdiction == "Unregistered", "missing server jurisdiction did not use stable fallback");

    const auto& rebuiltRemote = snapshot.systems[1];
    require(rebuiltRemote.id == 7 && rebuiltRemote.name == "Remote", "remote identity did not come from local catalog");
    require(rebuiltRemote.starType == "K4V" && rebuiltRemote.starsCount == 2, "remote static class did not come from local catalog");
    require(near(rebuiltRemote.positionLy.z, 13.0), "remote position did not come from local catalog");
    require(rebuiltRemote.jurisdiction == "Outer League", "authoritative jurisdiction overlay was not preserved");

    const auto& rebuiltObject = snapshot.objects.front();
    require(rebuiltObject.id == "relay-1" && rebuiltObject.name == "Relay One", "Galaxy object identity did not come from local catalog");
    require(rebuiltObject.objectType == "relay", "Galaxy object type did not come from local catalog");
    require(rebuiltObject.description == "Catalog object", "Galaxy object description did not come from local catalog");
    require(rebuiltObject.tags.size() == 2, "Galaxy object tags did not come from local catalog");
    require(near(rebuiltObject.positionLy.y, 5.0), "Galaxy object position did not come from local catalog");

    require(near(snapshot.universeTimeSeconds, 12345.0), "client rebuild changed authoritative universe epoch");
    require(snapshot.universeDate == "3026-01-01", "client rebuild changed authoritative universe date");

    std::cout << "[PASS] Galaxy map client catalog / server overlay contract\n";
    return 0;
}
