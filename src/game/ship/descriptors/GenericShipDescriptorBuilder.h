#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "src/game/catalog/ships/ShipDesignCatalog.h"
#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/presets/cobra/CobraPropulsionPresets.h"

// Generic ship descriptor: ONE builder turns any ship design of the catalog
// (canon/ship_designs.json -> assets/data/ships/ship_catalog_3126.json) into a
// runtime ShipDescriptor with real equipment modules and one hardware loadout.
//
// The ordinary-flight topology is one of the six fixed physical control models
// (Assisted / Newtonian x I / Y / X, CobraPropulsionPresets.h): a ship tunes
// ratings, never authors a private propulsion layout. The design supplies the
// numbers - drive thrust / input / efficiency / gimbal, generator rating and
// hot-pad size, radiators, payload, survival - and each installed unit becomes
// a damageable ModuleDescriptor whose id is "<installation code>.<n>". The
// descriptor holds string_views into the catalog, so the catalog must outlive
// it (ShipDesignCatalog::instance() is never destroyed).
//
// Not yet a flyable player ship for non-Cobra hulls: ShipCore still assumes the
// Cobra RCS nozzle presets, cockpit and mesh (see PROJECT_STATE "Ships").
namespace game::ship::descriptors
{
using game::catalog::ships::ShipDesign;
using game::catalog::ships::ShipDesignCatalog;
using game::catalog::ships::ShipDesignProduct;

[[nodiscard]] inline std::optional<game::ship::plant::PhysicalControlModelId> parsePhysicalControlModel(const std::string& name)
{
    using M = game::ship::plant::PhysicalControlModelId;
    if (name == "AssistedI") return M::AssistedI;
    if (name == "AssistedY") return M::AssistedY;
    if (name == "AssistedX") return M::AssistedX;
    if (name == "NewtonianI") return M::NewtonianI;
    if (name == "NewtonianY") return M::NewtonianY;
    if (name == "NewtonianX") return M::NewtonianX;
    return std::nullopt;
}

[[nodiscard]] inline std::string unitModuleId(const std::string& code, int index)
{
    return code + "." + std::to_string(index + 1);
}

// Bulk positions (armour tonnes, fluid tonnes, radiator field, C20 lock rails) are ONE module
// carrying their quantity; every other unit (drive, reactor, computer, lifeboat ...) is its own module.
[[nodiscard]] inline bool isBulkCategory(const std::string& category)
{
    return category == "armour" || category == "tank" || category == "radiator" || category == "container_rack";
}

[[nodiscard]] inline int moduleCountOf(const game::catalog::ships::ShipDesignInstalledItem& item)
{
    return isBulkCategory(item.category) ? 1 : item.quantity;
}

struct ShipDesignSchematicNodeView
{
    std::string repairLevel;
    bool lineReplaceable = false;
};

namespace detail
{
// repair doctrine of the designer (tools/colonization_lab/ship_designer.py REPAIR): factory items are not field-repairable
inline void applyRepairDoctrine(ModuleDescriptor& m, const std::string& repairLevel, bool lineReplaceable)
{
    m.repairable = repairLevel != "factory";
    m.salvageable = lineReplaceable;
    m.detachable = lineReplaceable;
}

[[nodiscard]] inline game::equipment::engine::EngineUnitDescriptor engineFrom(const ShipDesignProduct& p, const ShipDesign& s)
{
    game::equipment::engine::EngineUnitDescriptor e;
    e.engineTypeId = p.id;
    e.engineDisplayName = p.name;
    e.manufacturerName = p.manufacturer;
    e.propulsionPrinciple = game::equipment::engine::PropulsionPrinciple::FieldCoupling;
    e.structureMaterialId = "elite.material.engine.tial-ht.v1";
    e.nominalThrustNewtons = p.number("thrust_mn") * 1.0e6;
    e.nominalElectricalPowerMW = p.number("input_mw");
    e.nominalEfficiency01 = p.number("eff", 0.985);
    e.nominalGimbalRangeRad = p.number("gimbal_deg") * 3.14159265358979323846 / 180.0;
    e.nominalGimbalRateRadPerSec = e.nominalGimbalRangeRad > 0.5 ? 1000.0 : 0.6;
    e.thermalEmitter.outerDiameterM = s.driveEmitterDiameterM;
    e.thermalEmitter.surfaceTemperatureK = 2500.0;
    e.thermalEmitter.emissivity01 = 0.9;
    e.thermalEmitter.viewFactor01 = 0.9;
    e.thermalEmitter.materialId = "elite.material.engine.hfc-c.v1";
    e.nominalCoolantTemperatureK = 1050.0;
    e.nominalOperatingTemperatureK = 450.0;
    e.safeOperatingTemperatureK = 1250.0;
    e.thermalCapacityMJperK = 5.0 * p.massTonnes;
    // CR-1 calibration: 0.14004 MW/K housing -> coolant for 70.3 MW of waste
    const double wasteMW = e.nominalElectricalPowerMW * (1.0 - e.nominalEfficiency01);
    e.thermalConductanceMWperK = 0.14004 * wasteMW / (4850.0 * (1.0 - 0.9855));
    e.overhaulEquivalentHours = p.overhaulHours > 0.0 ? p.overhaulHours : 30000.0;
    e.endOfLifeEquivalentHours = p.lifeHours > 0.0 ? p.lifeHours : 50000.0;
    return e;
}

[[nodiscard]] inline game::equipment::reactor::EngineReactorDescriptor reactorFrom(const ShipDesignProduct& p, const ShipDesign& s)
{
    game::equipment::reactor::EngineReactorDescriptor d;
    d.typeId = p.id;
    d.displayName = p.name;
    d.manufacturer = p.manufacturer;
    d.ratedElectricalMW = p.number("mwe");
    d.nominalEfficiency01 = p.number("eff", 0.90);
    d.hotStandbyThermalMW = 2.0 * d.ratedElectricalMW / 5000.0;
    d.thermalCapacityMJPerK = 24.0 * p.massTonnes;
    d.padSideM = std::sqrt(std::max(1.0, s.reactorPadM2));
    d.idleTemperatureK = 900.0;
    d.startupTimeSeconds = p.number("startup_s", 420.0);
    d.operatingTemperatureK = 900.0;
    d.safeTemperatureK = 2100.0;
    d.criticalTemperatureK = 2400.0;
    d.overhaulEquivalentHours = p.overhaulHours > 0.0 ? p.overhaulHours : 30000.0;
    d.endOfLifeEquivalentHours = p.lifeHours > 0.0 ? p.lifeHours : 50000.0;
    d.failure.failureAtDamage01 = 0.95;
    d.maintenance.wearPerOperatingHour = 1.0 / d.endOfLifeEquivalentHours;
    d.maintenance.maintenanceWearThreshold01 = 0.20;
    return d;
}

[[nodiscard]] inline float crewLoadLimitG(const ShipDesign& s)
{
    if (s.shipClass == "interceptor") return 9.0f;
    if (s.shipClass == "colonist_transport") return 1.2f;
    if (s.passengers > 0) return s.shipClass == "surface_lifter" ? 3.0f : 1.5f;
    if (s.shipClass == "patrol_cutter" || s.shipClass == "escort_frigate" || s.shipClass == "recon_picket" ||
        s.shipClass == "line_combatant" || s.shipClass == "impact_defence_tug")
        return 6.0f;
    return 5.0f;
}
} // namespace detail

// Builds the descriptor; error (non-empty) explains why a design cannot be realised.
[[nodiscard]] inline std::optional<ShipDescriptor> buildGenericShipDescriptor(const ShipDesign& s, std::string& error)
{
    const auto& catalog = ShipDesignCatalog::instance();
    const auto model = parsePhysicalControlModel(s.physicalModel);
    if (!model)
    {
        error = "unknown physical model " + s.physicalModel;
        return std::nullopt;
    }
    const auto* driveItem = s.firstOf("drive");
    const auto* reactorItem = s.firstOf("reactor");
    const auto* serviceItem = s.firstOf("service_reactor");
    const auto* loopItem = s.firstOf("coolant_loop");
    if (!driveItem || !reactorItem || !serviceItem || !loopItem)
    {
        error = "design lacks drive / reactor / service reactor / coolant loop";
        return std::nullopt;
    }
    const auto* drive = catalog.findProduct(driveItem->productId);
    const auto* reactor = catalog.findProduct(reactorItem->productId);
    if (!drive || !reactor)
    {
        error = "unknown drive / reactor product";
        return std::nullopt;
    }

    const double L = s.envelopeMeters.x, W = s.envelopeMeters.y, H = s.envelopeMeters.z;
    const double mass = s.dryMassTonnes * 1000.0;
    const bool assisted = game::ship::plant::familyOf(*model) == game::ship::plant::PhysicalControlFamily::Assisted;

    ShipDescriptor d;
    d.typeId = ObjectType::None;                 // a catalog ship, not a legacy ObjectType hull
    d.identity = {s.designation, s.name};
    d.physicalControlModel = *model;
    d.localFlightControlLaw = game::ship::plant::nativeFlightLaw(*model);
    d.hud.name = s.designation;

    // ---- modules: one damageable module per installed unit; SPOF units are critical
    std::vector<std::string> driveModules, reactorModules, rcsModules;
    std::string serviceModule, loopModule;
    for (const auto& item : s.installed)
    {
        const auto* p = catalog.findProduct(item.productId);
        const ShipDesignSchematicNodeView node = [&] {
            for (const auto& n : s.schematicNodes)
                if (n.code == item.code)
                    return ShipDesignSchematicNodeView {n.repairLevel, n.lineReplaceable};
            return ShipDesignSchematicNodeView {"station", true};
        }();
        const bool spof = std::find(s.singlePointsOfFailure.begin(), s.singlePointsOfFailure.end(), item.code) != s.singlePointsOfFailure.end();
        for (int k = 0; k < moduleCountOf(item); ++k)
        {
            ModuleDescriptor m;
            m.moduleId = unitModuleId(item.code, k);
            m.parentModuleId = s.designation;
            m.subsystemId = item.category;
            m.moduleClass = item.productId;
            m.providedReplacementTags = {item.productId};
            m.acceptedReplacementTags = {item.productId};
            m.structuralKind = ModuleStructuralKind::Equipment;
            m.attachmentType = ModuleAttachmentType::Internal;
            m.destructible = true;
            m.isCritical = spof;
            detail::applyRepairDoctrine(m, node.repairLevel, node.lineReplaceable);
            if (item.category == "drive" && p)
            {
                m.engine = detail::engineFrom(*p, s);
                driveModules.push_back(m.moduleId);
            }
            else if (item.category == "reactor" && p)
            {
                m.engineReactor = detail::reactorFrom(*p, s);
                reactorModules.push_back(m.moduleId);
            }
            else if (item.category == "rcs")
                rcsModules.push_back(m.moduleId);
            else if (item.category == "service_reactor" && serviceModule.empty())
                serviceModule = m.moduleId;
            else if (item.category == "coolant_loop" && loopModule.empty())
                loopModule = m.moduleId;
            d.modules.push_back(std::move(m));
        }
    }

    // ---- loadout on the fixed physical topology
    ShipLoadoutDefinition loadout;
    loadout.shipPassportId = s.id;
    loadout.id = s.id + ":" + game::ship::plant::physicalControlModelName(*model);
    loadout.legacyModelId = *model;
    loadout.flightLaw = game::ship::plant::nativeFlightLaw(*model);
    loadout.propulsionSlots = game::ship::presets::cobra::propulsionSlots(*model);
    std::size_t nextDrive = 0, nextRcs = 0;
    for (const auto& slot : loadout.propulsionSlots)
    {
        if (slot.role == game::ship::plant::EngineSlotRole::ManoeuvreCluster)
            loadout.propulsionModuleIds.push_back(nextRcs < rcsModules.size() ? rcsModules[nextRcs++] : std::string());
        else
            loadout.propulsionModuleIds.push_back(nextDrive < driveModules.size() ? driveModules[nextDrive++] : std::string());
    }
    if (nextDrive != driveModules.size())
    {
        error = "drive units (" + std::to_string(driveModules.size()) + ") do not fill the " + s.physicalModel + " main slots";
        return std::nullopt;
    }
    const auto& groups = game::ship::presets::cobra::installationSlots(*model).engineReactors;
    if (groups.size() != reactorModules.size())
    {
        error = "propulsion reactors (" + std::to_string(reactorModules.size()) + ") do not match " + std::to_string(groups.size()) + " generator groups";
        return std::nullopt;
    }
    for (std::size_t i = 0; i < groups.size(); ++i)
    {
        ShipEngineReactorGroupDefinition group;
        group.slotId = groups[i].slotId;
        group.normalizedPositionBody = groups[i].normalizedPositionBody;
        for (const char* engine : groups[i].poweredEngineSlots)
            group.poweredEngineSlotIds.emplace_back(engine);
        group.reactorModuleId = reactorModules[i];
        loadout.engineReactorGroups.push_back(std::move(group));
        d.powerThermalInstallation.engineReactorModuleIds.push_back(reactorModules[i]);
    }
    d.powerThermalInstallation.shipReactorModuleId = serviceModule;
    d.powerThermalInstallation.commonCoolingModuleId = loopModule;
    d.physicalControl.slotModuleIds = loadout.propulsionModuleIds;
    for (std::size_t i = 0; i < loadout.propulsionSlots.size(); ++i)
    {
        if (loadout.propulsionSlots[i].role == game::ship::plant::EngineSlotRole::AftMain)
            d.mainPropulsion.aftEngineModuleIds.push_back(loadout.propulsionModuleIds[i]);
        else if (loadout.propulsionSlots[i].role == game::ship::plant::EngineSlotRole::ForeMain)
            d.mainPropulsion.foreEngineModuleIds.push_back(loadout.propulsionModuleIds[i]);
    }
    d.loadouts.push_back(std::move(loadout));

    // ---- rigid body and flight envelope
    auto& ph = d.physics;
    const double aftThrust = drive->number("thrust_mn") * 1.0e6 * static_cast<double>(d.mainPropulsion.aftEngineModuleIds.size());
    const double foreThrust = drive->number("thrust_mn") * 1.0e6 * static_cast<double>(d.mainPropulsion.foreEngineModuleIds.size());
    ph.massKg = mass;
    ph.pitchInertiaKgM2 = mass / 12.0 * (H * H + L * L);
    ph.yawInertiaKgM2 = mass / 12.0 * (W * W + L * L);
    ph.rollInertiaKgM2 = mass / 12.0 * (W * W + H * H);
    ph.physicalActuatorHalfExtentsMeters = {W * 0.5, H * 0.5, L * 0.5};
    ph.forwardMainEngineAvailable = aftThrust > 0.0;
    ph.reverseMainEngineAvailable = foreThrust > 0.0;
    ph.forwardMainEngineAccelerationMps2 = static_cast<float>(aftThrust / mass);
    ph.reverseMainEngineAccelerationMps2 = static_cast<float>(foreThrust / mass);
    const float gimbal = static_cast<float>(drive->number("gimbal_deg") * 3.14159265358979323846 / 180.0);
    ph.mainGimbalPitchLimitRad = gimbal;
    ph.mainGimbalYawLimitRad = gimbal;
    ph.mainGimbalRateRadPerSec = gimbal > 0.5f ? 1000.0f : 0.6f;
    ph.maxGs = detail::crewLoadLimitG(s);
    ph.maxLinearGs = ph.maxGs * 1.5f;
    ph.turnRadius = static_cast<float>(std::max(10.0, L));
    // Cobra (22 m) rates scaled by size: big hulls turn slower
    const float size = static_cast<float>(std::sqrt(22.2 / std::max(10.0, L)));
    ph.angularAccel = 3.0f * size * size;
    ph.angularDamping = 2.5f;
    ph.maxPitchRate = 2.5f * size;
    ph.maxYawRate = 2.5f * size;
    ph.maxRollRate = 3.0f * size;
    ph.maxCombatSpeed = 500.0f;                 // the universal controlled tactical envelope
    ph.maxCruiseSpeed = 29979245.0f;
    ph.throttleAccel = 5.0f;
    ph.autoLevelStrength = 0.0f;
    double rcsThrust = 0.0;
    for (const auto& item : s.installed)
        if (item.category == "rcs")
            if (const auto* p = catalog.findProduct(item.productId))
                rcsThrust += p->number("thrust_mn") * 1.0e6 * item.quantity;
    ph.manoeuvreThrusterAccel = static_cast<float>(rcsThrust / 3.0 / mass);
    ph.manoeuvreThrusterAuthorityExplicit = true;
    ph.strafeAccel = assisted ? static_cast<float>(aftThrust / mass * std::sin(std::min(gimbal, 1.3f)))
                              : ph.manoeuvreThrusterAccel;
    ph.strafeDamping = 6.0f;
    ph.maxStrafeSpeed = 80.0f;

    // ---- ship services: service reactor and radiators (legacy ship-wide thermal model)
    d.reactor = {s.serviceMWe, s.serviceMWe, 3500.0, 2500.0, 4500.0, 3500.0, 120.0, 0.9960, 0.9965, 0.9955};
    d.cooling.radiatorArea = s.radiatorAreaM2;
    d.cooling.panelCount = std::max(1, static_cast<int>(s.radiatorAreaM2 / 10.0));
    d.cooling.emissivity = s.radiatorEmissivity;
    d.cooling.maxTransferPower = s.coolantCapacityMW;
    d.cooling.nominalPower = s.coolantCapacityMW / 90.0;
    d.cooling.peakPower = s.coolantCapacityMW / 40.0;
    d.cooling.radiatorMaterialId = s.radiatorMaterial;

    // ---- payload, survival, bays
    d.storage.cargoMass = static_cast<int>(std::lround(s.cargoTonnes));
    d.storage.containerSlots = s.containerSlots;
    d.storage.containerPayloadTonnes = 20;
    d.survival.hasEscapePod = s.countOf("escape_capsule") + s.countOf("lifeboat") > 0;
    d.survival.hasNanokitBay = s.countOf("medbay") > 0;
    for (const auto& item : s.installed)
        if (const auto* p = catalog.findProduct(item.productId))
        {
            if (item.category == "drone_bay")
                d.docks.droneSlots += static_cast<int>(p->number("drones")) * item.quantity;
            else if (item.category == "hangar")
                d.docks.fighterSlots += static_cast<int>(p->number("craft_slots")) * item.quantity;
            else if (item.category == "boat_bay")
                d.docks.shuttleSlots += static_cast<int>(p->number("craft_slots")) * item.quantity;
            else if (item.category == "radar")
                d.systems.radarSlots += item.quantity;
            else if (item.category == "weapon")
                d.systems.weaponSlots += item.quantity;
            else if (item.category == "comm")
                d.systems.receiverSlots += item.quantity, d.systems.transmitterSlots += item.quantity;
        }
    d.systems.reactorSlots = static_cast<int>(reactorModules.size());
    d.systems.engineSlots = static_cast<int>(driveModules.size());

    // ---- geometry (placeholder primitive until a mesh exists)
    d.logicalDimensionsValue.length = static_cast<float>(L);
    d.logicalDimensionsValue.width = static_cast<float>(W);
    d.logicalDimensionsValue.height = static_cast<float>(H);
    d.logicalDimensionsValue.scaleReference = ScaleReference::Length;
    d.logicalDimensionsValue.enabled = true;
    d.meshSizeMeters = glm::vec3(static_cast<float>(W), static_cast<float>(H), static_cast<float>(L));
    d.boundingEllipsoid.radius = glm::vec3(static_cast<float>(W * 0.5), static_cast<float>(H * 0.5), static_cast<float>(L * 0.5));
    return d;
}

} // namespace game::ship::descriptors
