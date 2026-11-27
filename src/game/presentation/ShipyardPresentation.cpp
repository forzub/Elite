#include "src/game/presentation/ShipyardPresentation.h"

#include <cstdio>

#include "src/game/catalog/ships/ShipDesignCatalog.h"
#include "src/game/ship/descriptors/CatalogShipRegistry.h"
#include "src/game/localization/LocalizedMessage.h"

namespace game::presentation
{
namespace
{
using game::localization::Message;

std::string fmt(const char* format, double v)
{
    char buf[64];
    std::snprintf(buf, sizeof buf, format, v);
    return buf;
}

std::string grouped(double v)
{
    // 12 345 678
    char buf[48];
    std::snprintf(buf, sizeof buf, "%.0f", v);
    std::string s = buf, out;
    const int n = static_cast<int>(s.size());
    for (int i = 0; i < n; ++i)
    {
        out.push_back(s[static_cast<std::size_t>(i)]);
        if ((n - i - 1) % 3 == 0 && i != n - 1 && s[0] != '-')
            out.push_back(' ');
    }
    return out;
}

// the passport label of an installed item's category: a language-neutral Message (the renderer renders it)
std::string categoryLabel(const std::string& c)
{
    static const char* const known[] = {
        "drive", "reactor", "service_reactor", "radiator", "coolant_loop", "jump_core", "tank", "rcs", "computer", "radar",
        "comm", "transponder", "ecm", "sensor_mast", "life_support", "escape_capsule", "cockpit_capsule", "lifeboat", "medbay",
        "container_rack", "passenger_module", "train_module", "cryo_tank", "bulk_hold", "docking_port", "boat_bay", "drone_bay",
        "hangar", "tow_rig", "workshop", "heat_shield", "landing_gear", "weapon", "armour"};
    for (const char* k : known)
        if (c == k)
            return Message(std::string("service.panel.shipyard.category.") + k);
    return Message("service.panel.shipyard.category.equipment");
}

ShipyardModelCard makeCard(const game::catalog::ships::ShipDesign& s, const ShipDescriptor& d, bool passportExpanded)
{
    const auto& catalog = game::catalog::ships::ShipDesignCatalog::instance();
    ShipyardModelCard c;
    c.valid = true;
    c.passportId = s.id;
    c.name = s.name;
    c.manufacturer = s.manufacturer;
    c.shipClass = s.shipClass;
    c.purpose = s.purpose;
    c.envelopeMeters = {static_cast<float>(s.envelopeMeters.x), static_cast<float>(s.envelopeMeters.y), static_cast<float>(s.envelopeMeters.z)};
    for (const auto& b : d.primitiveBlocks)
        c.preview.push_back({b.category, b.centerMeters, b.halfSizeMeters});
    c.dryTonnes = d.physics.massKg / 1000.0;
    c.serviceTonnes = s.serviceTonnes;
    c.grossTonnes = s.grossTonnes;
    c.crew = s.crew;
    c.passengers = s.passengers;
    c.cargoTonnes = s.cargoTonnes;
    c.containerSlots = s.containerSlots;

    // propulsion: the installed drive modules of the runtime descriptor
    std::string driveName;
    for (const auto& id : d.mainPropulsion.aftEngineModuleIds)
        for (const auto& m : d.modules)
            if (m.moduleId == id && m.engine)
            {
                c.thrustMN += m.engine->nominalThrustNewtons / 1e6;
                if (const auto* p = catalog.findProduct(m.moduleClass))
                    driveName = p->model;
            }
    const int mains = static_cast<int>(d.mainPropulsion.aftEngineModuleIds.size() + d.mainPropulsion.foreEngineModuleIds.size());
    c.propulsionScheme = std::string(game::ship::plant::physicalControlModelName(d.physicalControlModel)) + ": " +
                         std::to_string(mains) + " × " + driveName;
    c.serviceAccelerationMps2 = c.serviceTonnes > 0.0 ? c.thrustMN * 1e6 / (c.serviceTonnes * 1000.0) : 0.0;
    c.grossAccelerationMps2 = c.grossTonnes > 0.0 ? c.thrustMN * 1e6 / (c.grossTonnes * 1000.0) : 0.0;
    for (const auto& m : d.modules)
        if (m.engineReactor)
            c.propulsionPowerMW += m.engineReactor->ratedElectricalMW;
    c.servicePowerMW = d.reactor.nominalPowerMW;
    c.jumpRatingTonnes = d.catalog.jumpCoreRatingTonnes;
    c.jump = Message("service.panel.shipyard.jump_none");
    if (const auto* core = s.firstOf("jump_core"))
        if (const auto* p = catalog.findProduct(core->productId))
            c.jump = Message("service.panel.shipyard.jump_core").arg("model", p->model).arg("tonnes", grouped(c.jumpRatingTonnes));
    c.radarRangeKm = d.catalog.radarRangeMeters / 1000.0;
    for (const auto* r : s.allOf("radar"))
        if (const auto* p = catalog.findProduct(r->productId); p && p->number("range_km") * 1000.0 == d.catalog.radarRangeMeters)
            c.radar = Message("service.panel.shipyard.radar_model").arg("model", p->model).arg("km", grouped(c.radarRangeKm));
    c.priceListCr = s.priceListCr;

    // labels and values are language-neutral Messages (LocalizedMessage.h): the renderer shows them in the UI language
    const auto label = [](const char* key) { return Message(std::string("service.panel.shipyard.spec.") + key).str(); };
    c.mainSpecs = {
        {label("dimensions"), Message("service.panel.shipyard.value.dimensions")
                                  .arg("x", fmt("%.0f", s.envelopeMeters.x)).arg("y", fmt("%.0f", s.envelopeMeters.y))
                                  .arg("z", fmt("%.1f", s.envelopeMeters.z))},
        {label("mass"), Message("service.panel.shipyard.value.mass")
                            .arg("dry", grouped(c.dryTonnes)).arg("service", grouped(c.serviceTonnes)).arg("gross", grouped(c.grossTonnes))},
        {label("crew"), std::to_string(c.crew) + " / " + std::to_string(c.passengers)},
        {label("cargo"), c.containerSlots > 0 ? Message("service.panel.shipyard.value.cargo_c20")
                                                    .arg("tonnes", grouped(c.cargoTonnes)).arg("slots", c.containerSlots).str()
                                              : Message("service.panel.shipyard.value.cargo").arg("tonnes", grouped(c.cargoTonnes)).str()},
        {label("propulsion"), c.propulsionScheme},
        {label("thrust"), Message("service.panel.shipyard.value.thrust")
                              .arg("thrust", fmt("%.1f", c.thrustMN))
                              .arg("service", fmt("%.2f", c.serviceAccelerationMps2 / 9.80665))
                              .arg("gross", fmt("%.2f", c.grossAccelerationMps2 / 9.80665))},
        {label("jump"), c.jump},
        {label("radar"), c.radar},
        {label("reactors"), Message("service.panel.shipyard.value.reactors")
                                .arg("drive", grouped(c.propulsionPowerMW)).arg("service", grouped(c.servicePowerMW))},
        {label("price_new"), grouped(c.priceListCr) + " Cr"},
    };
    if (passportExpanded)
    {
        c.passportSpecs.push_back({label("passport"), s.id});
        c.passportSpecs.push_back({label("purpose"), s.purpose});
        c.passportSpecs.push_back({label("service_life"), Message("service.panel.shipyard.value.service_life").arg("years", s.serviceLifeYears)});
        c.passportSpecs.push_back({label("radiators"), Message("service.panel.shipyard.value.radiators")
                                                           .arg("area", grouped(s.radiatorAreaM2)).arg("mw", grouped(s.coolantCapacityMW))});
        for (const auto& item : s.installed)
        {
            const auto* p = catalog.findProduct(item.productId);
            c.passportSpecs.push_back({categoryLabel(item.category),
                                       (p ? p->name : item.productId) + (item.quantity > 1 ? " × " + std::to_string(item.quantity) : std::string())});
        }
    }
    c.offers = game::ship::refit::devShipOffers(s);
    return c;
}
} // namespace

ShipyardPresentation buildShipyardPresentation(const std::string& selectedPassportId, bool passportExpanded)
{
    ShipyardPresentation out;
    out.passportExpanded = passportExpanded;
    const auto& catalog = game::catalog::ships::ShipDesignCatalog::instance();
    for (const auto& s : catalog.ships())
    {
        if (!game::ship::descriptors::findCatalogShip(s.id))
            continue;      // only models the runtime can actually build
        ShipyardModelRow row;
        row.passportId = s.id;
        row.name = s.name;
        row.manufacturer = s.manufacturer;
        row.shipClass = s.shipClass;
        row.priceListCr = s.priceListCr;
        out.models.push_back(std::move(row));
    }
    for (std::size_t i = 0; i < out.models.size(); ++i)
        if (out.models[i].passportId == selectedPassportId)
            out.selected = static_cast<int>(i);
    if (out.selected < 0 && !out.models.empty())
        out.selected = 0;
    if (out.selected >= 0)
    {
        auto& row = out.models[static_cast<std::size_t>(out.selected)];
        row.selected = true;
        const auto* s = catalog.findShip(row.passportId);
        const auto* d = game::ship::descriptors::findCatalogShip(row.passportId);
        if (s && d)
            out.card = makeCard(*s, *d, passportExpanded);
    }
    return out;
}
} // namespace game::presentation
