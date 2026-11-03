// Offline scripted check of the System Map information layer (fourth pass):
// a human must find the Sol hubs without knowing ids or cubes.
//
//   EliteSystemMapInfoCheck [--sites path] [--world path]
//
// Checks on the GENERATED runtime copies (galaxy_details/world_sites/sites_3126.json,
// initial_world_state.json):
//  - the system summary (nothing selected) names the Earth orbital hubs and the Main Belt hub
//    "Перевал Цереры" by name, with owner / operator facts;
//  - selecting a hub (by the marker's object id, as a click does) gives type, owner, operator,
//    jurisdiction, docks, traffic and its routes in the right panel;
//  - selecting Earth (body) lists the hubs that belong to it;
//  - marker <-> world object identity: every runtime hub has exactly one site record bound to it,
//    every route end is a real site or runtime hub, every relay / belt anchor has a placement;
//  - the Ceres hub stands at its belt anchor (heliocentric distance 2.5-3.0 AU).
// Exit code 1 on any failure.
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "src/game/presentation/SystemMapInfoBuilder.h"
#include "src/world/celestial/CelestialOrbitKinematics.h"
#include "src/world/celestial/WorldSitesCatalog.h"

namespace
{
int failures = 0;

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "PASS " : "FAIL ") << what << "\n";
    if (!ok)
        ++failures;
}

bool contains(const std::vector<game::presentation::SystemMapInfoLine>& lines, const std::string& needle)
{
    for (const auto& l : lines)
        if (l.label.find(needle) != std::string::npos || l.value.find(needle) != std::string::npos)
            return true;
    return false;
}

std::string field(const std::vector<game::presentation::SystemMapInfoLine>& lines, const std::string& label)
{
    for (const auto& l : lines)
        if (l.label == label)
            return l.value;
    return {};
}
}

int main(int argc, char** argv)
{
    std::string sitesPath = "src/assets/data/galaxy_details/world_sites/sites_3126.json";
    std::string worldPath = "src/assets/data/initial_world_state.json";
    for (int i = 1; i + 1 < argc; i += 2)
    {
        const std::string a = argv[i];
        if (a == "--sites") sitesPath = argv[i + 1];
        else if (a == "--world") worldPath = argv[i + 1];
    }
    world::celestial::WorldSitesCatalog catalog;
    check(catalog.load(sitesPath), "world sites runtime copy loads (" + sitesPath + ")");
    std::ifstream in(worldPath);
    nlohmann::json world;
    in >> world;

    using game::presentation::SystemMapInfoInput;
    std::string title;
    std::vector<game::presentation::SystemMapInfoLine> lines;

    // 1. nothing selected: the summary tells what is here, by name
    SystemMapInfoInput summary;
    summary.systemId = 0;
    summary.catalog = &catalog;
    game::presentation::buildSystemMapInfo(summary, title, lines);
    check(title == "КАРТА СИСТЕМЫ", "panel title is КАРТА СИСТЕМЫ");
    check(contains(lines, "Главные политические акторы") && contains(lines, "Экономика") && contains(lines, "Текущее состояние") &&
              contains(lines, "Отношение к вам"),
          "system summary has actors / economy / state / relation to the player");
    check(contains(lines, "Перевал Цереры"), "summary names the Main Belt hub 'Перевал Цереры' (findable without ids)");
    check(contains(lines, "Высокий") && contains(lines, "Тихий"), "summary names the Earth orbital hubs");
    check(!contains(lines, "site.000") && !contains(lines, "earth_hub."), "no internal ids in the summary text");

    // 2. a belt hub selected (the marker's object id, as a click activates it)
    SystemMapInfoInput ceres = summary;
    ceres.selectedObjectId = "site.000.sol.ceres";
    game::presentation::buildSystemMapInfo(ceres, title, lines);
    check(contains(lines, "Перевал Цереры") && !field(lines, "Оператор").empty() && !field(lines, "Владелец").empty(),
          "Ceres hub: name, owner and operator in the right panel");
    check(field(lines, "Доки").find("площадок") != std::string::npos && field(lines, "Доки").find("модулей HexDock") != std::string::npos && !field(lines, "Трафик").empty() && !field(lines, "Юрисдикция").empty(),
          "Ceres hub: docks as sectors -> HexDock modules -> pads, traffic and jurisdiction");
    check(contains(lines, "Маршруты"), "Ceres hub: its routes are listed");

    // 3. a replicated Earth hub selected by its runtime id
    SystemMapInfoInput tikhiy = summary;
    tikhiy.selectedObjectId = "earth_hub.tikhiy";
    game::presentation::buildSystemMapInfo(tikhiy, title, lines);
    check(contains(lines, "Тихий") && field(lines, "Оператор").find("Тихоокеанская") != std::string::npos, "Earth hub Тихий resolves through its runtime binding");

    // 4. Earth selected: its hubs
    world::celestial::SystemMapSnapshot snap;
    snap.systemId = 0;
    world::celestial::SystemMapBody earth;
    earth.id = "system_0.Sol.Земля";
    earth.name = "Земля";
    earth.radiusKm = 6371.0;
    earth.positionAu = glm::dvec3(1.0, 0.0, 0.0);
    snap.bodies.push_back(earth);
    SystemMapInfoInput body = summary;
    body.system = &snap;
    body.selectedBodyId = earth.id;
    game::presentation::buildSystemMapInfo(body, title, lines);
    check(contains(lines, "Крупные хабы") && contains(lines, "Тихий") && contains(lines, "Канаверал") && contains(lines, "Поселения и местные объекты"),
          "Earth body: its major hubs listed, settlements only counted");

    // 5. marker <-> world object identity
    std::set<std::string> runtimeHubs;
    for (const auto& h : world["orbital_hubs"])
        runtimeHubs.insert(h["id"].get<std::string>());
    int bound = 0;
    for (const auto& id : runtimeHubs)
    {
        const auto* site = catalog.findSite(0, id);
        bound += site && site->runtimeBinding.find("[" + id + "]") != std::string::npos ? 1 : 0;
    }
    check(bound == static_cast<int>(runtimeHubs.size()) && runtimeHubs.size() == 8,
          "8 runtime hubs (7 Earth complexes + Ceres), each with exactly one bound world-site record (" + std::to_string(bound) + ")");
    int badEnds = 0;
    for (const auto& r : catalog.routesInSystem(0))
        for (const auto& end : {r.origin, r.destination})
            badEnds += catalog.findSite(0, end) || runtimeHubs.count(end) ? 0 : 1;
    check(!catalog.routesInSystem(0).empty() && badEnds == 0,
          std::to_string(catalog.routesInSystem(0).size()) + " routes, every end is a real hub (" + std::to_string(badEnds) + " bad)");
    int relaysPlaced = 0, anchorsPlaced = 0;
    for (const auto& s : catalog.sitesInSystem(0))
    {
        relaysPlaced += s.type == "nav_relay" && s.placement != world::celestial::WorldSitePlacementKind::None ? 1 : 0;
        anchorsPlaced += s.type == "belt_anchor" && s.placement == world::celestial::WorldSitePlacementKind::Heliocentric ? 1 : 0;
    }
    check(relaysPlaced == static_cast<int>(catalog.relaysInSystem(0).size()) && relaysPlaced > 0,
          std::to_string(relaysPlaced) + " relays are world objects with a placement");
    check(anchorsPlaced == 4, "4 belt anchors (Ceres, Vesta, Pallas, Hygiea) are map objects");

    // 5b. map budget on the data the client draws: major (level A) markers + replicated hubs
    int major = static_cast<int>(runtimeHubs.size());
    int earthMajor = 0;
    for (const auto& id : runtimeHubs)
        earthMajor += id.rfind("earth", 0) == 0 ? 1 : 0;
    for (const auto& s : catalog.sitesInSystem(0))
        if (s.mapLevel == "system" && s.runtimeBinding.empty() && s.type != "nav_relay" && s.type != "belt_anchor" && s.level == "A")
            ++major;
    check(major <= 30 && major >= 24, "Sol major hubs drawn on the System Map: " + std::to_string(major) + " / 30");
    check(earthMajor >= 6 && earthMajor <= 8, "Earth major complexes: " + std::to_string(earthMajor) + " (6-8)");
    SystemMapInfoInput vys = summary;
    vys.selectedObjectId = "earth_orbital_hub";
    game::presentation::buildSystemMapInfo(vys, title, lines);
    check(field(lines, "Терминалы").find("Байконур") != std::string::npos, "Vysokiy panel shows its terminals (former Baikonur station is a terminal)");

    // 6. the Ceres hub stands at its anchor
    const auto* ce = catalog.findSite(0, "site.000.sol.ceres");
    double rAu = 0.0;
    if (ce && ce->placement == world::celestial::WorldSitePlacementKind::Heliocentric)
    {
        world::celestial::CelestialBodyDefinition b;
        b.distanceAu = ce->elements.aAu;
        b.eccentricity = ce->elements.e;
        b.inclinationDeg = ce->elements.iDeg;
        b.longitudeAscendingNodeDeg = ce->elements.nodeDeg;
        b.argumentOfPeriapsisDeg = ce->elements.periDeg;
        b.meanAnomalyAtEpochDeg = ce->elements.meanAnomalyEpochDeg;
        b.orbitalPeriodDays = 365.25 * std::pow(ce->elements.aAu, 1.5);
        glm::dvec3 p(0.0), v(0.0);
        world::celestial::keplerStateAu(b, 0.0, p, v);
        rAu = glm::length(p);
    }
    check(rAu > 2.5 && rAu < 3.0, "Ceres hub at its anchor: " + std::to_string(rAu) + " AU from the Sun");

    std::cout << (failures ? "SYSTEM MAP INFO CHECK: FAILED (" + std::to_string(failures) + ")" : "SYSTEM MAP INFO CHECK: PASS") << "\n";
    return failures ? 1 : 0;
}
