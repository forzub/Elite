#include "src/game/presentation/ShipyardPresentation.h"

#include <cstdio>

#include "src/game/catalog/ships/ShipDesignCatalog.h"
#include "src/game/ship/descriptors/CatalogShipRegistry.h"

namespace game::presentation
{
namespace
{
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

const char* categoryRu(const std::string& c)
{
    static const std::pair<const char*, const char*> names[] = {
        {"drive", "главный двигатель"}, {"reactor", "двигательный реактор"}, {"service_reactor", "служебный реактор"},
        {"radiator", "радиаторы"}, {"coolant_loop", "контур охлаждения"}, {"jump_core", "ядро прыжка"}, {"tank", "баки"},
        {"rcs", "маневровая система"}, {"computer", "бортовой компьютер"}, {"radar", "радар"}, {"comm", "связь"},
        {"transponder", "транспондер"}, {"ecm", "РЭБ"}, {"sensor_mast", "сенсорная мачта"}, {"life_support", "жизнеобеспечение"},
        {"escape_capsule", "спасательная капсула"}, {"lifeboat", "спасательная шлюпка"}, {"medbay", "медотсек"},
        {"container_rack", "места C20"}, {"passenger_module", "пассажирские модули"}, {"train_module", "модули поезда"},
        {"cryo_tank", "криоцистерны"}, {"bulk_hold", "грузовой отсек"}, {"docking_port", "стыковочный узел"},
        {"boat_bay", "ангар катера"}, {"drone_bay", "отсек дронов"}, {"hangar", "ангар"}, {"tow_rig", "буксирная рама"},
        {"workshop", "мастерская"}, {"heat_shield", "теплозащита"}, {"landing_gear", "посадочные опоры"},
        {"weapon", "вооружение"}, {"armour", "броня"}};
    for (const auto& [k, v] : names)
        if (c == k)
            return v;
    return "оборудование";
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
    c.jump = "нет (внутрисистемный)";
    if (const auto* core = s.firstOf("jump_core"))
        if (const auto* p = catalog.findProduct(core->productId))
            c.jump = p->model + ", " + grouped(c.jumpRatingTonnes) + " т";
    c.radarRangeKm = d.catalog.radarRangeMeters / 1000.0;
    for (const auto* r : s.allOf("radar"))
        if (const auto* p = catalog.findProduct(r->productId); p && p->number("range_km") * 1000.0 == d.catalog.radarRangeMeters)
            c.radar = p->model + ", " + grouped(c.radarRangeKm) + " км";
    c.priceListCr = s.priceListCr;

    c.mainSpecs = {
        {"Габариты", fmt("%.0f", s.envelopeMeters.x) + " × " + fmt("%.0f", s.envelopeMeters.y) + " × " + fmt("%.1f", s.envelopeMeters.z) + " м"},
        {"Масса сухая / рабочая / полная", grouped(c.dryTonnes) + " / " + grouped(c.serviceTonnes) + " / " + grouped(c.grossTonnes) + " т"},
        {"Экипаж / пассажиры", std::to_string(c.crew) + " / " + std::to_string(c.passengers)},
        {"Груз", grouped(c.cargoTonnes) + " т" + (c.containerSlots > 0 ? ", C20 × " + std::to_string(c.containerSlots) : std::string())},
        {"Схема двигателей", c.propulsionScheme},
        {"Тяга / ускорение", fmt("%.1f", c.thrustMN) + " МН, " + fmt("%.2f", c.serviceAccelerationMps2 / 9.80665) + " g рабочая, " +
                                  fmt("%.2f", c.grossAccelerationMps2 / 9.80665) + " g полная"},
        {"Прыжок", c.jump},
        {"Радар", c.radar},
        {"Реакторы", grouped(c.propulsionPowerMW) + " МВт тяга, " + grouped(c.servicePowerMW) + " МВт служебный"},
        {"Цена нового", grouped(c.priceListCr) + " Cr"},
    };
    if (passportExpanded)
    {
        c.passportSpecs.push_back({"Паспорт", s.id});
        c.passportSpecs.push_back({"Назначение", s.purpose});
        c.passportSpecs.push_back({"Ресурс", std::to_string(s.serviceLifeYears) + " лет"});
        c.passportSpecs.push_back({"Радиаторы", grouped(s.radiatorAreaM2) + " м², контуры " + grouped(s.coolantCapacityMW) + " МВт"});
        for (const auto& item : s.installed)
        {
            const auto* p = catalog.findProduct(item.productId);
            c.passportSpecs.push_back({categoryRu(item.category),
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
