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
//      from canon/political_order.json) loads, the 3126 polities appear under
//      their current names (no retired "Holy Zion"), bloc channels resolve.
#include <cmath>
#include <iostream>
#include <set>
#include <sstream>

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

    // ---- 6. F8 Trade runtime data smoke (political / economic order 3126)
    {
        const auto& eco = game::economy::EconomyModel::instance();
        check(eco.loaded() && eco.goods().size() >= 30, "economy runtime loads (goods " + std::to_string(eco.goods().size()) + ")");
        std::size_t hubs = 0;
        bool retired = false;
        auto ownerIn = [&](int sys, const std::string& name, const game::economy::HubEconomyInfo** first) {
            bool hit = false;
            for (const auto* h : eco.hubsOfSystem(sys))
                if (h->ownerName.find(name) != std::string::npos)
                {
                    hit = true;
                    if (first && !*first)
                        *first = h;
                }
            return hit;
        };
        for (int sys = 0; sys < 400; ++sys)
            for (const auto* h : eco.hubsOfSystem(sys))
            {
                ++hubs;
                for (const auto* text : {&h->name, &h->ownerName, &h->operatorName, &h->description})
                    if (text->find("Сион") != std::string::npos)   // "Сион"
                        retired = true;
            }
        check(hubs >= 500, "economy hubs in star systems: " + std::to_string(hubs));
        check(!retired, "no hub shows the retired name of Leo 83 (Novo-Thebaid)");
        const game::economy::HubEconomyInfo *cog = nullptr, *mecca = nullptr, *india = nullptr, *cit = nullptr, *svarog = nullptr,
                                            *thebaid = nullptr, *firm = nullptr, *zone = nullptr;
        const bool names = ownerIn(8, "Новый Шанхай", &cog)      // Новый Шанхай
                           && ownerIn(4, "Новая Мекка", &mecca)        // Новая Мекка
                           && ownerIn(14, "Новая Индия", &india)      // Новая Индия
                           && ownerIn(3, "Цитадель", &cit)                          // Цитадель
                           && ownerIn(60, "Красный Форпост", &svarog)   // Красный Форпост
                           && ownerIn(45, "Ново-Фиваида", &thebaid)   // Ново-Фиваида
                           && ownerIn(15, "Твердыня", &firm);                       // Твердыня
        for (const auto* h : eco.hubsOfSystem(12))
            if (h->bloc.rfind("zone:", 0) == 0 && !zone)
                zone = h;
        check(names && zone, "hubs of New Shanghai, New Mecca, New India, Citadel, Svarog, Novo-Thebaid, Firmament and the Philomorphosis zone found");
        if (names && zone)
        {
            std::cout << "  blocs: Cognium " << cog->bloc << ", Mecca " << mecca->bloc << ", Citadel " << cit->bloc << ", Svarog " << svarog->bloc
                      << ", Thebaid " << thebaid->bloc << ", Firmament " << firm->bloc << ", zone " << zone->bloc << "\n";
            check(mecca->bloc == "southern_circle" && india->bloc == "southern_circle" && eco.channel(mecca->id, india->id) == "open",
                  "Southern circle: Mecca - New India open");
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
