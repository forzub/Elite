// EliteServicePagesCheck: F6 Shipyard and F7 Repair & Refit page models are
// built from ONE source - the ship catalog / runtime descriptors and the real
// ship instance (ObjectModuleRuntime -> replicated module snapshots ->
// server RepairRefitPreview publication).
//   1. Shipyard lists every catalog model once;
//   2. no hand-duplicated numbers: card values equal what the runtime
//      descriptor / passport say, recomputed independently here;
//   3. Repair & Refit lists the real module ids of the ship instance;
//   4. a destroyed radar shows as failed (module + sensors group);
//   5. the page carries the server preview: radar restored in "after";
//   6. F8 Trade data smoke: the runtime economy (assets/data/economy, generated
//      from canon/political_order.json) loads; hubs are found by stable actor id
//      and their owner names equal the naming resolver (assets/data/naming,
//      generated from canon/naming.json); no retired name; bloc channels resolve.
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>

#include "src/game/economy/EconomyModel.h"
#include "src/game/presentation/RepairRefitPresentation.h"
#include "src/game/presentation/ShipyardPresentation.h"
#include "src/game/ship/ShipInitData.h"
#include "src/game/ship/core/ShipCore.h"
#include "src/game/ship/descriptors/CatalogShipRegistry.h"
#include "src/game/simulation/ObjectModuleSnapshotBuilder.h"

namespace
{
int failures = 0;

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "PASS " : "FAIL ") << what << "\n";
    if (!ok)
        ++failures;
}

std::vector<game::simulation::ObjectModuleSnapshot> replicate(const game::ship::core::ShipCore& core)
{
    std::vector<game::simulation::ObjectModuleSnapshot> out;
    for (const auto& m : core.descriptor().modules)
        out.push_back(game::simulation::makeObjectModuleSnapshot(m, core.moduleRuntime().findModule(m.moduleId)));
    return out;
}
} // namespace

int main()
{
    using namespace game::presentation;
    const auto& catalog = game::catalog::ships::ShipDesignCatalog::instance();

    // ---- 1. every catalog model, once
    const auto yard = buildShipyardPresentation({}, true);
    std::set<std::string> ids;
    for (const auto& m : yard.models)
        ids.insert(m.passportId);
    std::set<std::string> catalogIds;
    for (const auto& s : catalog.ships())
        catalogIds.insert(s.id);
    check(yard.models.size() == catalog.ships().size() && ids == catalogIds && yard.models.size() == 21,
          "Shipyard lists " + std::to_string(yard.models.size()) + " models = the " + std::to_string(catalog.ships().size()) +
              " catalog designs, each once");

    // ---- 2. card numbers come from the runtime descriptor / passport
    int mismatches = 0;
    for (const auto& s : catalog.ships())
    {
        const auto page = buildShipyardPresentation(s.id, true);
        const auto& c = page.card;
        const auto* d = game::ship::descriptors::findCatalogShip(s.id);
        if (!c.valid || !d)
        {
            ++mismatches;
            continue;
        }
        double thrust = 0.0, power = 0.0;
        for (const auto& id : d->mainPropulsion.aftEngineModuleIds)
            for (const auto& m : d->modules)
                if (m.moduleId == id && m.engine)
                    thrust += m.engine->nominalThrustNewtons / 1e6;
        for (const auto& m : d->modules)
            if (m.engineReactor)
                power += m.engineReactor->ratedElectricalMW;
        const bool same = std::abs(c.thrustMN - thrust) < 1e-9 && std::abs(c.dryTonnes - d->physics.massKg / 1000.0) < 1e-9 &&
                          c.serviceTonnes == s.serviceTonnes && c.grossTonnes == s.grossTonnes &&
                          std::abs(c.radarRangeKm - d->catalog.radarRangeMeters / 1000.0) < 1e-9 && c.priceListCr == s.priceListCr &&
                          std::abs(c.propulsionPowerMW - power) < 1e-9 && c.jumpRatingTonnes == d->catalog.jumpCoreRatingTonnes &&
                          c.preview.size() == d->primitiveBlocks.size() && c.offers.size() == 4 && !c.passportSpecs.empty();
        if (!same)
        {
            ++mismatches;
            std::cout << "     mismatch " << s.id << "\n";
        }
    }
    check(mismatches == 0, "every Shipyard card value equals the runtime descriptor / passport (thrust, masses, radar, "
                           "power, jump, price, preview blocks, 4 seller offers)");
    {
        const auto& o = yard.card.offers;
        bool ladder = o.size() == 4 && o[0].sellerId == "manufacturer" && o[0].priceCr == yard.card.priceListCr;
        for (std::size_t i = 1; ladder && i < o.size(); ++i)
            ladder = o[i].priceCr < o[0].priceCr * 0.8 + 1e-6 && o[i].priceCr >= o[i].scrapFloorCr - 1e-6 && o[i].ageYears > 0.0;
        check(ladder, "seller offers: a new ship at the passport list price, every used one below -20 % and above its scrap floor");
    }

    // ---- 3-5. Repair & Refit from a real instance
    const auto* c120 = game::ship::descriptors::findCatalogShip("bharat.ship.c-120.konveyer");
    auto core = std::make_unique<game::ship::core::ShipCore>();
    core->init(ShipRole::NPC, *c120, glm::vec3(0.0f), ShipInitData {}, glm::mat4(1.0f));
    const std::string radar = c120->catalog.radarModuleIds.front();
    core->debugDestroyModuleById(radar);
    core->refreshRuntimeState();
    const auto modules = replicate(*core);
    game::simulation::ShipRefitSnapshot refit;
    refit.owned = true;
    refit.ownerActor = 1;
    refit.shipName = "C-120 test";
    refit.preview = game::ship::refit::previewRefit(*core, {game::ship::refit::RefitOperationKind::Repair, radar, {}});
    refit.previewValid = true;
    refit.previewSerial = 1;
    const auto page = buildRepairRefitPresentation({{EntityId {7}, c120, &modules, &refit}}, EntityId {7}, {}, radar, 0);
    std::set<std::string> pageIds, runtimeIds;
    for (const auto& m : page.modules)
        pageIds.insert(m.moduleId);
    for (const auto& m : core->moduleRuntime().modules())
        runtimeIds.insert(m.moduleId);
    check(page.valid && pageIds == runtimeIds && page.modules.size() == c120->modules.size(),
          "Repair & Refit lists the " + std::to_string(page.modules.size()) + " real module ids of the instance (ObjectModuleRuntime)");
    const RefitModuleView* radarView = nullptr;
    for (const auto& m : page.modules)
        if (m.moduleId == radar)
            radarView = &m;
    int sensorsFailed = 0;
    for (const auto& g : page.groups)
        if (g.key == "sensors")
            sensorsFailed = g.failed;
    check(radarView && radarView->failed() && radarView->selected && sensorsFailed == 1,
          "destroyed radar: module " + radar + " failed (" + (radarView ? radarView->state : std::string("?")) +
              "), radar & comms group shows 1 failure");
    check(page.previewForSelection && page.preview.ok && !page.preview.before.radar && page.preview.after.radar &&
              page.preview.after.radarRangeKm == 2000.0,
          "the page carries the server preview: radar restored, range 0 -> 2000 km");
    // another module selected: the radar preview is not shown for it
    const auto other = buildRepairRefitPresentation({{EntityId {7}, c120, &modules, &refit}}, EntityId {7}, {},
                                                    c120->mainPropulsion.aftEngineModuleIds.front(), 0);
    check(!other.previewForSelection, "a preview is shown only for the module it was computed for");

    // ---- 6. F8 Trade runtime data smoke (political / economic order 3126), by stable id:
    //      hub.actor (unit id) -> naming resolver (assets/data/naming/names_3126.json) -> display name
    {
        const auto& eco = game::economy::EconomyModel::instance();
        check(eco.loaded() && eco.goods().size() >= 30, "economy runtime loads (goods " + std::to_string(eco.goods().size()) + ")");
        nlohmann::json names;
        for (const std::string path : {"assets/data/naming/names_3126.json", "src/assets/data/naming/names_3126.json"})
        {
            std::ifstream f(path);
            if (f)
            {
                names = nlohmann::json::parse(f, nullptr, false);
                break;
            }
        }
        const bool namesLoaded = names.is_object() && names.contains("names") && names["names"].is_object();
        check(namesLoaded, "naming resolver table loads (" + std::to_string(namesLoaded ? names["names"].size() : 0) + " ids)");
        auto resolve = [&](const std::string& id, const char* field) {
            if (!namesLoaded || !names["names"].contains(id) || !names["names"][id].contains(field) || !names["names"][id][field].is_string())
                return std::string();
            return names["names"][id][field].get<std::string>();
        };
        std::size_t hubs = 0, orphans = 0;
        for (int sys = 0; sys < 400; ++sys)
            for (const auto* h : eco.hubsOfSystem(sys))
            {
                ++hubs;
                if (!h->actor.empty() && resolve("polity:" + h->actor, "ru").empty())
                    ++orphans;
            }
        check(hubs >= 500, "economy hubs in star systems: " + std::to_string(hubs));
        check(orphans == 0, "every hub actor id resolves to a name (" + std::to_string(orphans) + " unresolved)");
        // a hub of the actor in its home system whose owner shows the resolver's current colony name
        // (a province runs under its actor: Celestial Rampart hubs belong to Xinhe, owner = the province colony)
        auto hubOf = [&](int sys, const std::string& actor, const game::economy::HubEconomyInfo** out, const std::string& unit = std::string()) {
            const std::string colony = resolve("colony:" + (unit.empty() ? actor : unit), "ru");
            for (const auto* h : eco.hubsOfSystem(sys))
                if (h->actor == actor && !colony.empty() && h->ownerName == colony)
                {
                    *out = h;
                    return true;
                }
            return false;
        };
        const game::economy::HubEconomyInfo *cog = nullptr, *tauranga = nullptr, *india = nullptr, *cit = nullptr, *svarog = nullptr,
                                            *thebaid = nullptr, *firm = nullptr, *zone = nullptr;
        const bool found = hubOf(8, "tau_ceti_cognium", &cog) && hubOf(4, "lalande_mecca", &tauranga) && hubOf(14, "hd20794_newindia", &india) &&
                           hubOf(3, "wolf359_citadel", &cit) && hubOf(60, "proxima_outpost", &svarog) && hubOf(45, "leo83_zion", &thebaid) &&
                           hubOf(15, "eps_eri_harmony", &firm, "hd40307_firmament");
        for (const auto* h : eco.hubsOfSystem(12))
            if (h->bloc.rfind("zone:", 0) == 0 && !zone)
                zone = h;
        check(found && zone, "hubs found by actor id; owner shows the resolver name: " + resolve("colony:tau_ceti_cognium", "ru") + ", " +
                                 resolve("colony:lalande_mecca", "ru") + " (" + resolve("colony:lalande_mecca", "native") + "), " +
                                 resolve("colony:hd20794_newindia", "ru") + ", " + resolve("colony:wolf359_citadel", "ru") + " (" +
                                 resolve("colony:wolf359_citadel", "native") + "), " + resolve("colony:proxima_outpost", "ru") + ", " +
                                 resolve("colony:leo83_zion", "ru") + ", " + resolve("colony:hd40307_firmament", "ru") + "; quarantine zone");
        // retired authoring names (canon/naming.json retired_authoring_names) never reach a hub
        bool retired = false;
        for (const char* old : {"Мекк", "Сион", "Нуджум", "AL-QUDS", "Вторая Эллада", "Скьёльдбург"})
            for (int sys = 0; sys < 400; ++sys)
                for (const auto* h : eco.hubsOfSystem(sys))
                    for (const auto* text : {&h->name, &h->ownerName, &h->operatorName, &h->description})
                        if (text->find(old) != std::string::npos)
                            retired = true;
        check(!retired, "no hub shows a retired name");
        if (found && zone)
        {
            std::cout << "  blocs: Cognium " << cog->bloc << ", Tauranga " << tauranga->bloc << ", Citadel " << cit->bloc << ", Svarog " << svarog->bloc
                      << ", Thebaid " << thebaid->bloc << ", Firmament " << firm->bloc << ", zone " << zone->bloc << "\n";
            check(tauranga->bloc == "southern_circle" && india->bloc == "southern_circle" && eco.channel(tauranga->id, india->id) == "open",
                  "Southern circle: Tauranga - New India open");
            check(cit->bloc == "citadel_compact" && svarog->bloc == "citadel_compact", "Citadel and Svarog in the Citadel pact");
            check(firm->bloc == "heavenly_pact", "Celestial Firmament (province of Xin He) runs under the Celestial pact");
            check(thebaid->bloc == "nonaligned:leo83_zion", "Novo-Thebaid non-aligned");
            check(eco.channel(cog->id, cit->id) != "open" && eco.channel(cog->id, cit->id) != "closed", "Cognium - Citadel: civilian trade possible, not open (" + eco.channel(cog->id, cit->id) + ")");
            check(eco.channel(zone->id, cog->id) == "closed", "quarantine zone closed to ordinary trade");
        }
    }

    std::cout << (failures ? "SERVICE PAGES CHECK: FAILED (" + std::to_string(failures) + ")" : "SERVICE PAGES CHECK: PASS") << "\n";
    return failures ? 1 : 0;
}
