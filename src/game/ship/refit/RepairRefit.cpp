#include "src/game/ship/refit/RepairRefit.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "src/game/catalog/ships/ShipDesignCatalog.h"
#include "src/game/navigation/DynamicMotionState.h"
#include "src/game/ship/ShipInitData.h"
#include "src/game/ship/core/ShipCore.h"
#include "src/game/ship/descriptors/GenericShipDescriptorBuilder.h"
#include "src/game/ship/refit/ShipMarketRules.h"

namespace game::ship::refit
{
namespace
{
using game::catalog::ships::ShipDesign;
using game::catalog::ships::ShipDesignCatalog;
using game::catalog::ships::ShipDesignInstalledItem;
using game::catalog::ships::ShipDesignProduct;

const ModuleDescriptor* findModule(const ShipDescriptor& d, const std::string& id)
{
    for (const auto& m : d.modules)
        if (m.moduleId == id)
            return &m;
    return nullptr;
}

ModuleDescriptor* findModule(ShipDescriptor& d, const std::string& id)
{
    for (auto& m : d.modules)
        if (m.moduleId == id)
            return &m;
    return nullptr;
}

const ShipDesign* designOf(const ShipDescriptor& d)
{
    return d.catalog.valid() ? ShipDesignCatalog::instance().findShip(d.catalog.passportId) : nullptr;
}

// The installation position a module belongs to: the longest installed code
// that prefixes the module id ("<code>.<n>", "<code>.<node>...").
const ShipDesignInstalledItem* itemOf(const ShipDesign& s, const std::string& moduleId)
{
    const ShipDesignInstalledItem* best = nullptr;
    for (const auto& item : s.installed)
        if (moduleId.size() > item.code.size() && moduleId.compare(0, item.code.size(), item.code) == 0 &&
            moduleId[item.code.size()] == '.' && (!best || item.code.size() > best->code.size()))
            best = &item;
    return best;
}

// The serviceable unit behind a module: an RCS nozzle belongs to its cluster block.
std::string productOfModule(const ShipDescriptor& d, const std::string& moduleId)
{
    const auto* m = findModule(d, moduleId);
    if (!m)
        return {};
    if (ShipDesignCatalog::instance().findProduct(m->moduleClass))
        return m->moduleClass;
    if (!m->parentModuleId.empty())
        if (const auto* p = findModule(d, m->parentModuleId); p && ShipDesignCatalog::instance().findProduct(p->moduleClass))
            return p->moduleClass;
    return {};
}

const char* stateName(world::modules::ObjectModuleRuntimeState s)
{
    using S = world::modules::ObjectModuleRuntimeState;
    switch (s)
    {
        case S::Attached: return "attached";
        case S::Disabled: return "disabled";
        case S::Destroyed: return "destroyed";
        case S::Detached: return "detached";
        case S::Hanging: return "hanging";
    }
    return "unknown";
}

double functionalDamage01(const game::equipment::EquipmentDamageState& a)
{
    return std::clamp(a.laser01 + a.impact01 + a.thermal01 + a.radiation01 + a.wear01 + a.internalFailure01, 0.0, 1.0);
}

// Full forward burn on the production control path, same harness as the
// ship catalog check: returns the mean forward acceleration of the last half.
double fullForwardBurn(core::ShipCore& ship, double seconds)
{
    auto& tr = ship.transform();
    tr.motion.mode = game::navigation::MotionMode::HubTactical;
    tr.motion.travelFrame.systemId = 0;
    tr.motion.travelFrame.frameId = "refit-preview";
    tr.motion.travelFrame.originMeters = glm::dvec3(0.0);
    tr.motion.travelFrame.localToWorldBasis = glm::dmat3(1.0);
    tr.motion.travelFrame.valid = true;
    tr.motion.localPositionMeters = glm::dvec3(0.0);
    tr.motion.localVelocityMps = glm::dvec3(0.0);
    tr.motion.worldVelocityMps = glm::dvec3(0.0);
    tr.orientation = glm::mat4(1.0f);
    tr.pitchRate = tr.yawRate = tr.rollRate = 0.0f;
    const bool assisted = game::ship::plant::familyOf(ship.state().construction.physicalControlModel) ==
                          game::ship::plant::PhysicalControlFamily::Assisted;
    const double dt = 1.0 / 30.0;
    const int steps = std::max(2, static_cast<int>(seconds / dt));
    glm::dvec3 vHalf {0.0};
    for (int i = 0; i < steps; ++i)
    {
        auto& c = ship.control();
        c.targetSpeedRate = 1.0f;
        c.assistedMaxSpeedCommand = assisted;
        ship.updateTranslationControl(static_cast<float>(dt));
        ship.propagateMotionOrientation(static_cast<float>(dt));
        (void)ship.propagateHubTacticalTranslation(dt);
        ship.updateSystems(static_cast<float>(dt));
        if (i == steps / 2)
            vHalf = tr.motion.localVelocityMps;
    }
    return glm::length(tr.motion.localVelocityMps - vHalf) / (dt * (steps - steps / 2 - 1));
}

void addDelta(std::vector<RefitDelta>& out, const char* key, const char* label, const char* unit, double before, double after,
              double tolerance)
{
    if (std::abs(after - before) > tolerance)
        out.push_back({key, label, unit, before, after, false});
}

void addFlag(std::vector<RefitDelta>& out, const char* key, const char* label, bool before, bool after)
{
    if (before != after)
        out.push_back({key, label, "", before ? 1.0 : 0.0, after ? 1.0 : 0.0, true});
}

// Swaps the installed product of a catalog module in a descriptor COPY
// (preview of a different product; the live ship is never touched).
bool swapProduct(ShipDescriptor& d, const std::string& moduleId, const ShipDesignProduct& p, const ShipDesign& s)
{
    auto* m = findModule(d, moduleId);
    if (!m)
        return false;
    m->moduleClass = p.id;
    m->providedReplacementTags = {p.id};
    m->acceptedReplacementTags = {p.id};
    if (m->placement.valid)
        m->placement.massKg = p.massTonnes * 1000.0;
    if (m->engine)
        m->engine = game::ship::descriptors::detail::engineFrom(p, s);
    if (m->engineReactor)
        m->engineReactor = game::ship::descriptors::detail::reactorFrom(p, s);
    if (p.category == "rcs")
        for (auto& child : d.modules)
            if (child.parentModuleId == moduleId && child.engine)
                child.engine = game::ship::descriptors::detail::nozzleFrom(p, 1.0);
    if (p.category == "radar" && d.defaultEquipment.radar)
    {
        d.defaultEquipment.radar->maxRange = p.number("range_km") * 1000.0;
        d.defaultEquipment.radar->powerConsumption = p.number("power_mw", d.defaultEquipment.radar->powerConsumption);
        d.catalog.radarRangeMeters = d.defaultEquipment.radar->maxRange;
    }
    return true;
}

std::unique_ptr<core::ShipCore> throwaway(const ShipDescriptor& d, const core::ShipCore& live)
{
    auto ship = std::make_unique<core::ShipCore>();
    ship->init(ShipRole::NPC, d, glm::vec3(0.0f), ShipInitData {}, glm::mat4(1.0f));
    ship->adoptInstanceState(live);
    return ship;
}
} // namespace

RefitModuleFacts moduleFacts(const ShipDescriptor& d, const std::string& moduleId)
{
    RefitModuleFacts f;
    f.moduleId = moduleId;
    const auto* m = findModule(d, moduleId);
    if (!m)
        return f;
    f.category = m->subsystemId;
    f.repairable = m->repairable;
    f.lineReplaceable = m->detachable;
    f.massTonnes = m->placement.valid ? m->placement.massKg / 1000.0 : 0.0;
    f.productId = productOfModule(d, moduleId);
    f.repairLevel = m->repairable ? "station" : "factory";
    if (const auto* p = ShipDesignCatalog::instance().findProduct(f.productId))
    {
        f.productName = p->name;
        f.category = p->category;
        f.priceCr = p->priceCr;
        f.powerMW = p->number("power_mw", p->number("input_mw"));
    }
    if (const auto* s = designOf(d))
        if (const auto* item = itemOf(*s, moduleId))
            for (const auto& node : s->schematicNodes)
                if (node.code == item->code)
                {
                    f.repairLevel = node.repairLevel;
                    f.replaceHours = node.replaceHours;
                    f.lineReplaceable = node.lineReplaceable;
                }
    return f;
}

std::string replacementIncompatibility(const ShipDescriptor& d, const std::string& moduleId, const std::string& productId)
{
    const auto facts = moduleFacts(d, moduleId);
    if (facts.productId.empty())
        return "модуль не из каталога оборудования: замена на другое изделие невозможна";
    const auto* p = ShipDesignCatalog::instance().findProduct(productId);
    if (!p)
        return "неизвестное изделие " + productId;
    const auto* installed = ShipDesignCatalog::instance().findProduct(facts.productId);
    if (!installed || p->category != installed->category)
        return "несовместимая категория: " + (installed ? installed->category : std::string("?")) + " ← " + p->category;
    if (p->massTonnes > installed->massTonnes * 1.5 + 3.0)   // mount / space allowance
        return "не помещается: " + std::to_string(static_cast<int>(p->massTonnes)) + " т против " +
               std::to_string(static_cast<int>(installed->massTonnes)) + " т места установки";
    const bool assisted = game::ship::plant::familyOf(d.physicalControlModel) == game::ship::plant::PhysicalControlFamily::Assisted;
    if (p->category == "drive")
    {
        if (assisted && p->number("gimbal_deg") < 30.0)
            return "Assisted требует широкого подвеса сопла (не менее 30°), у изделия " +
                   std::to_string(static_cast<int>(p->number("gimbal_deg"))) + "°";
        // the generator group must power it
        for (const auto& m : d.modules)
            if (m.engineReactor && p->number("input_mw") > m.engineReactor->ratedElectricalMW * 1.02)
                return "генератор группы " + std::to_string(static_cast<int>(m.engineReactor->ratedElectricalMW)) +
                       " МВт не питает двигатель " + std::to_string(static_cast<int>(p->number("input_mw"))) + " МВт";
    }
    if (p->category == "reactor")
        for (const auto& m : d.modules)
            if (m.engine && m.subsystemId == "drive" && m.engine->nominalElectricalPowerMW > p->number("mwe") * 1.02)
                return "реактор " + std::to_string(static_cast<int>(p->number("mwe"))) + " МВт не питает двигатель " +
                       std::to_string(static_cast<int>(m.engine->nominalElectricalPowerMW)) + " МВт";
    return {};
}

std::vector<RefitReplacement> compatibleReplacements(const ShipDescriptor& d, const std::string& moduleId)
{
    std::vector<RefitReplacement> out;
    const auto facts = moduleFacts(d, moduleId);
    const auto* installed = ShipDesignCatalog::instance().findProduct(facts.productId);
    if (!installed)
        return out;
    // the catalog's products of this category (every product any design installs)
    for (const auto& s : ShipDesignCatalog::instance().ships())
        for (const auto& item : s.installed)
        {
            if (item.category != installed->category)
                continue;
            if (std::any_of(out.begin(), out.end(), [&](const auto& r) { return r.productId == item.productId; }))
                continue;
            if (!replacementIncompatibility(d, moduleId, item.productId).empty())
                continue;
            const auto* p = ShipDesignCatalog::instance().findProduct(item.productId);
            out.push_back({p->id, p->name, p->manufacturer, p->priceCr, p->id == installed->id});
        }
    std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.sameProduct > b.sameProduct; });
    return out;
}

RefitShipMetrics measureShip(core::ShipCore& ship, const std::string& moduleId, double burnSeconds)
{
    RefitShipMetrics m;
    ship.refreshRuntimeState();
    ship.refreshDeviceState();
    const auto cap = ship.capabilities();
    const auto& d = ship.descriptor();
    m.valid = cap.valid;
    m.massTonnes = ship.currentMassKg() / 1000.0;
    const auto& p = cap.effectivePhysics;
    m.forwardAccelerationMps2 = p.forwardMainEngineAvailable ? p.forwardMainEngineAccelerationMps2 : 0.0;
    m.reverseAccelerationMps2 = p.reverseMainEngineAvailable ? p.reverseMainEngineAccelerationMps2 : 0.0;
    m.rcsAccelerationMps2 = std::min({cap.strafeAccelerationMps2.positive, cap.strafeAccelerationMps2.negative,
                                      cap.liftAccelerationMps2.positive, cap.liftAccelerationMps2.negative});
    m.angularAccelerationRadPerSec2 = std::min({cap.pitchAccelerationRadPerSec2.positive, cap.pitchAccelerationRadPerSec2.negative,
                                                cap.yawAccelerationRadPerSec2.positive, cap.yawAccelerationRadPerSec2.negative});
    const auto* loadout = d.findLoadout(ship.state().construction.physicalControlModel);
    const auto& slots = ship.state().actuators.propulsionSlots;
    if (loadout)
        for (std::size_t i = 0; i < loadout->propulsionSlots.size() && i < slots.size(); ++i)
            if (loadout->propulsionSlots[i].role != game::ship::plant::EngineSlotRole::ManoeuvreCluster)
            {
                ++m.mainEnginesInstalled;
                if (slots[i].operational && game::ship::plant::effectiveSlotThrust01(slots[i]) > 1e-6)
                    ++m.mainEnginesOperational;
            }
    const auto& reactors = d.powerThermalInstallation.engineReactorModuleIds;
    for (std::size_t g = 0; g < reactors.size(); ++g)
    {
        m.propulsionPowerAvailableMW += ship.engineReactorAvailableMW(g);
        if (const auto* r = findModule(d, reactors[g]); r && r->engineReactor)
            m.propulsionPowerRatedMW += r->engineReactor->ratedElectricalMW;
    }
    m.servicePowerOutputMW = ship.reactor().getCurrentOutput();
    const auto& dev = ship.devices();
    m.radar = dev.radar;
    m.radarRangeKm = dev.radarRangeMeters / 1000.0;
    m.receiver = dev.receiver;
    m.transmitter = dev.transmitter;
    m.transponder = dev.transponder;
    m.lifeSupport = dev.lifeSupport;
    m.computer = dev.computer;
    m.jumpCore = dev.jumpCore;
    m.jumpMassWithinRating = dev.jumpMassWithinRating;
    m.docksOperational = dev.dockPortsOperational;
    if (const auto* st = ship.moduleRuntime().findModule(moduleId))
    {
        m.moduleState = stateName(st->state);
        m.moduleHealth01 = st->maxHealth > 0.0f ? st->health / st->maxHealth : 0.0;
        m.moduleDamage01 = functionalDamage01(st->equipmentDamage.accumulated);
        m.moduleWear01 = st->equipmentDamage.accumulated.wear01;
    }

    // what the hardware does under load: a fixed-step full burn
    if (burnSeconds > 0.0)
    {
        m.burnSeconds = burnSeconds;
        m.burnAccelerationMps2 = fullForwardBurn(ship, burnSeconds);
        m.engineThermalMargin01 = 1.0;
        m.reactorThermalMargin01 = 1.0;
        for (const auto& def : d.modules)
        {
            const auto* st = ship.moduleRuntime().findModule(def.moduleId);
            if (!st)
                continue;
            if (def.engine && def.subsystemId != "propulsion.rcs" && def.engine->safeOperatingTemperatureK > 0.0)
            {
                m.maxEngineTemperatureK = std::max(m.maxEngineTemperatureK, st->engineOperating.temperatureK);
                m.engineThermalMargin01 = std::min(m.engineThermalMargin01,
                                                   1.0 - st->engineOperating.temperatureK / def.engine->safeOperatingTemperatureK);
            }
            if (def.engineReactor && def.engineReactor->safeTemperatureK > 0.0)
            {
                m.maxReactorTemperatureK = std::max(m.maxReactorTemperatureK, st->engineReactorOperating.temperatureK);
                m.reactorThermalMargin01 = std::min(m.reactorThermalMargin01,
                                                    1.0 - st->engineReactorOperating.temperatureK / def.engineReactor->safeTemperatureK);
            }
        }
        m.serviceCoolantK = ship.serviceCoolantTemperatureK();
        m.serviceCoolantCriticalK = ship.serviceCoolantCriticalTemperatureK();
        m.propulsionCoolantK = ship.propulsionCoolantTemperatureK();
        m.emergencyCoolingMW = ship.emergencyBridgeTransferMW();
    }
    return m;
}

RefitPreview previewRefit(const core::ShipCore& live, const RefitOperation& operation)
{
    RefitPreview out;
    out.operation = operation;
    const auto& d = live.descriptor();
    if (!findModule(d, operation.moduleId))
    {
        out.error = "модуль " + operation.moduleId + " не установлен на этом корабле";
        return out;
    }
    const auto facts = moduleFacts(d, operation.moduleId);
    const bool otherProduct = operation.kind == RefitOperationKind::Replace && !operation.productId.empty() &&
                              operation.productId != facts.productId;
    if (operation.kind == RefitOperationKind::Remove || operation.kind == RefitOperationKind::Install)
    {
        out.error = "снятие и установка — следующий этап";
        return out;
    }
    if (operation.kind == RefitOperationKind::Repair && !facts.repairable)
    {
        out.error = "ремонт на месте невозможен (" + facts.repairLevel + "): только замена";
        return out;
    }
    if (otherProduct)
    {
        out.error = replacementIncompatibility(d, operation.moduleId, operation.productId);
        if (!out.error.empty())
            return out;
    }

    constexpr double BurnSeconds = 8.0;
    // BEFORE: the live state on a throwaway ship
    auto before = throwaway(d, live);
    out.before = measureShip(*before, operation.moduleId, BurnSeconds);

    // AFTER: the same, with the operation applied by the same calls apply uses
    std::unique_ptr<ShipDescriptor> swapped;
    const ShipDescriptor* afterDescriptor = &d;
    if (otherProduct)
    {
        const auto* s = designOf(d);
        const auto* p = ShipDesignCatalog::instance().findProduct(operation.productId);
        swapped = std::make_unique<ShipDescriptor>(d);
        if (!s || !p || !swapProduct(*swapped, operation.moduleId, *p, *s))
        {
            out.error = "не удалось собрать конфигурацию с изделием " + operation.productId;
            return out;
        }
        afterDescriptor = swapped.get();
    }
    auto after = throwaway(*afterDescriptor, live);
    const bool replace = operation.kind == RefitOperationKind::Replace;
    if (!after->serviceModule(operation.moduleId, replace))
    {
        out.error = replace ? "замена невозможна: сначала восстановите несущий модуль" : "ремонт невозможен: модуль снят или несущий модуль разрушен";
        return out;
    }
    out.after = measureShip(*after, operation.moduleId, BurnSeconds);
    after.reset();
    before.reset();

    auto& dl = out.deltas;
    const auto& b = out.before;
    const auto& a = out.after;
    addDelta(dl, "module_health", "состояние модуля", "%", b.moduleHealth01 * 100.0, a.moduleHealth01 * 100.0, 0.05);
    addDelta(dl, "module_damage", "функциональные повреждения", "%", b.moduleDamage01 * 100.0, a.moduleDamage01 * 100.0, 0.05);
    addDelta(dl, "module_wear", "износ", "%", b.moduleWear01 * 100.0, a.moduleWear01 * 100.0, 0.05);
    addDelta(dl, "propulsion_power", "мощность двигательных генераторов", "МВт", b.propulsionPowerAvailableMW, a.propulsionPowerAvailableMW, 0.5);
    addDelta(dl, "main_engines", "главные двигатели в строю", "", b.mainEnginesOperational, a.mainEnginesOperational, 0.5);
    addDelta(dl, "forward_accel", "ускорение вперёд", "м/с²", b.forwardAccelerationMps2, a.forwardAccelerationMps2, 0.005);
    addDelta(dl, "reverse_accel", "торможение / назад", "м/с²", b.reverseAccelerationMps2, a.reverseAccelerationMps2, 0.005);
    addDelta(dl, "burn_accel", "разгон в прогоне 8 с", "м/с²", b.burnAccelerationMps2, a.burnAccelerationMps2, 0.01);
    addDelta(dl, "rcs_accel", "маневровые (слабейшая ось)", "м/с²", b.rcsAccelerationMps2, a.rcsAccelerationMps2, 0.001);
    addDelta(dl, "angular_accel", "угловое ускорение", "рад/с²", b.angularAccelerationRadPerSec2, a.angularAccelerationRadPerSec2, 0.0005);
    addDelta(dl, "engine_margin", "тепловой запас двигателей", "%", b.engineThermalMargin01 * 100.0, a.engineThermalMargin01 * 100.0, 0.2);
    addDelta(dl, "reactor_margin", "тепловой запас реакторов", "%", b.reactorThermalMargin01 * 100.0, a.reactorThermalMargin01 * 100.0, 0.2);
    addDelta(dl, "propulsion_coolant", "двигательный контур", "K", b.propulsionCoolantK, a.propulsionCoolantK, 0.5);
    addDelta(dl, "service_coolant", "служебный контур", "K", b.serviceCoolantK, a.serviceCoolantK, 0.5);
    addDelta(dl, "emergency_cooling", "аварийный теплообменник", "МВт", b.emergencyCoolingMW, a.emergencyCoolingMW, 0.05);
    addDelta(dl, "service_power", "служебный реактор, выдача", "МВт", b.servicePowerOutputMW, a.servicePowerOutputMW, 0.05);
    addDelta(dl, "mass", "масса корабля", "т", b.massTonnes, a.massTonnes, 0.005);
    addDelta(dl, "radar_range", "дальность радара", "км", b.radarRangeKm, a.radarRangeKm, 0.5);
    addFlag(dl, "radar", "радар", b.radar, a.radar);
    addFlag(dl, "receiver", "приёмник", b.receiver, a.receiver);
    addFlag(dl, "transmitter", "передатчик", b.transmitter, a.transmitter);
    addFlag(dl, "transponder", "транспондер", b.transponder, a.transponder);
    addFlag(dl, "life_support", "жизнеобеспечение", b.lifeSupport, a.lifeSupport);
    addFlag(dl, "computer", "бортовой компьютер", b.computer, a.computer);
    addFlag(dl, "jump_core", "ядро прыжка", b.jumpCore, a.jumpCore);
    addFlag(dl, "jump_mass", "прыжок с текущей массой", b.jumpMassWithinRating, a.jumpMassWithinRating);
    addDelta(dl, "docks", "стыковочные узлы и ангары в строю", "", b.docksOperational, a.docksOperational, 0.5);

    // cost: passport product price + installation repair doctrine (data-driven rates)
    const auto& rules = ShipMarketRules::instance();
    out.cost.repairLevel = facts.repairLevel;
    const double rate = rules.labourCrPerHour.count(facts.repairLevel) ? rules.labourCrPerHour.at(facts.repairLevel) : 140.0;
    const double baseHours = facts.replaceHours > 0.0 ? facts.replaceHours
                             : (rules.defaultHours.count(facts.repairLevel) ? rules.defaultHours.at(facts.repairLevel) : 6.0);
    double price = facts.priceCr;
    if (otherProduct)
        if (const auto* p = ShipDesignCatalog::instance().findProduct(operation.productId))
            price = p->priceCr;
    if (replace)
    {
        out.cost.labourHours = baseHours * rules.replaceLabourX;
        out.cost.materialCr = price;
    }
    else
    {
        const double broken = std::clamp(std::max(1.0 - b.moduleHealth01, b.moduleDamage01 - b.moduleWear01), 0.0, 1.0);
        out.cost.labourHours = std::max(0.5, baseHours * broken);
        out.cost.materialCr = price * rules.repairMaterialShare01 * broken;
    }
    out.cost.labourCr = out.cost.labourHours * rate;
    out.cost.totalCr = out.cost.labourCr + out.cost.materialCr;

    out.ok = true;
    out.applicable = !otherProduct;
    out.applyNote = otherProduct ? "установка другого изделия требует экземплярной конфигурации корабля (следующий этап); предпросмотр точный"
                                 : "DEV: применяется к живому кораблю без оплаты";
    return out;
}

std::string applyRefitOperation(core::ShipCore& live, const RefitOperation& operation)
{
    const auto facts = moduleFacts(live.descriptor(), operation.moduleId);
    if (operation.kind == RefitOperationKind::Replace && !operation.productId.empty() && operation.productId != facts.productId)
        return "установка другого изделия требует экземплярной конфигурации корабля (следующий этап)";
    if (operation.kind == RefitOperationKind::Repair && !facts.repairable)
        return "ремонт на месте невозможен: только замена";
    if (operation.kind != RefitOperationKind::Repair && operation.kind != RefitOperationKind::Replace)
        return "снятие и установка — следующий этап";
    if (!live.serviceModule(operation.moduleId, operation.kind == RefitOperationKind::Replace))
        return "операция отклонена модульным runtime";
    return {};
}
} // namespace game::ship::refit
