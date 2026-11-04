#pragma once

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "src/game/catalog/ships/ShipDesignCatalog.h"
#include "src/game/equipment/data/radars.h"
#include "src/game/equipment/data/receivers.h"
#include "src/game/equipment/data/transmitters.h"
#include "src/game/equipment/engine/ManoeuvreEnginePreset.h"
#include "src/game/equipment/engine/presets/CobrettyCR1VectorMainDrive.h"
#include "src/game/equipment/engine/presets/KobraN1AxialMainDrive.h"
#include "src/game/equipment/rcs/presets/CobraArgonRcsTank.h"
#include "src/game/equipment/reactor/presets/CobraCobrettyER5PropulsionReactor.h"
#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/ShipLoadoutTopology.h"
#include "src/game/ship/plant/CanonicalActuatorLayouts.h"

// Generic ship descriptor: ONE builder turns any ship design of the catalog
// (canon/ship_designs.json -> assets/data/ships/ship_catalog_3126.json) into a
// runtime ShipDescriptor that ShipCore executes - Cobrett included.
//
//  - Structure: hull.root -> hull.fore / hull.mid / hull.aft -> equipment.
//    Every installed unit is a damageable module of its section (support link
//    to it); bulk positions (armour, tanks, radiators, C20 rails) are one
//    module each. Every module carries its passport placement (box, mass).
//  - Actuators: the slot STRUCTURE is one of the six fixed physical models
//    (plant::canonical); the mount COORDINATES of mains, generator groups and
//    the 24-nozzle Newtonian RCS bank are the passport's own positions,
//    registered as this loadout's actuator topology (lever arms r x F).
//    An RCS cluster block is the parent module of its node's three nozzles.
//  - Mass: dry mass, centre of mass and the inertia tensor are summed from
//    the module placements; ShipCore adds the live loads (cargo, people,
//    fluids, consumables) and drops detached modules.
//  - Devices: radar (passport range), receiver, transmitter, transponder,
//    life support, jump core, service reactor, coolant loop are bound to
//    their modules (ShipCatalogRuntime).
// The descriptor holds string_views into the catalog (engine passports), so
// the catalog must outlive it (ShipDesignCatalog::instance() never dies).
namespace game::ship::descriptors
{
using game::catalog::ships::ShipDesign;
using game::catalog::ships::ShipDesignCatalog;
using game::catalog::ships::ShipDesignInstalledItem;
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

[[nodiscard]] inline int moduleCountOf(const ShipDesignInstalledItem& item)
{
    return isBulkCategory(item.category) ? 1 : item.quantity;
}

inline constexpr const char* HullRootModuleId = "hull.root";

// RCS module ids: a nozzle unit is "<code>.<node>_<axis>"; a cluster block "<code>.<node>"
// owns its node's three nozzles (the leading "_x" binds the node's control slot).
[[nodiscard]] inline std::string rcsNozzleModuleId(const std::string& code, std::string_view nozzleId)
{
    return code + "." + std::string(nozzleId);
}

namespace detail
{
inline constexpr double Pi = 3.14159265358979323846;

[[nodiscard]] inline std::string sectionOf(double zMeters, double lengthMeters)
{
    if (zMeters < -lengthMeters / 6.0) return "hull.fore";
    if (zMeters > lengthMeters / 6.0) return "hull.aft";
    return "hull.mid";
}

// repair doctrine of the designer (tools/colonization_lab/ship_designer.py REPAIR): factory items are not field-repairable
inline void applyRepairDoctrine(ModuleDescriptor& m, const std::string& repairLevel, bool lineReplaceable)
{
    m.repairable = repairLevel != "factory";
    m.salvageable = lineReplaceable;
    m.detachable = lineReplaceable;
}

[[nodiscard]] inline game::equipment::engine::EngineUnitDescriptor engineFrom(const ShipDesignProduct& p, const ShipDesign& s)
{
    // Products that ARE an existing runtime drive use its passport verbatim (Cobrett).
    const std::string preset = p.text("runtime_preset");
    if (preset == "elite.engine.cobretty.cr1-vector.v1")
        return game::equipment::engine::presets::CobrettyCR1VectorMainDrive();
    if (preset == "elite.engine.kobra.n1-axial.v1")
        return game::equipment::engine::presets::KobraN1AxialMainDrive();

    game::equipment::engine::EngineUnitDescriptor e;
    e.engineTypeId = p.id;
    e.engineDisplayName = p.name;
    e.manufacturerName = p.manufacturer;
    e.propulsionPrinciple = game::equipment::engine::PropulsionPrinciple::FieldCoupling;
    e.structureMaterialId = "elite.material.engine.tial-ht.v1";
    e.nominalThrustNewtons = p.number("thrust_mn") * 1.0e6;
    e.nominalElectricalPowerMW = p.number("input_mw");
    e.nominalEfficiency01 = p.number("eff", 0.985);
    e.nominalGimbalRangeRad = p.number("gimbal_deg") * Pi / 180.0;
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

// One fixed-direction RCS nozzle rated `units` x the product's per-axis thrust.
[[nodiscard]] inline game::equipment::engine::EngineUnitDescriptor nozzleFrom(const ShipDesignProduct& p, double units)
{
    auto e = game::equipment::engine::cobraManoeuvreNozzlePreset();   // the field-coupled nozzle thermal/wear model
    if (p.text("runtime_preset") == e.engineTypeId)
        return e;                                                      // Cobrett FC-RCS: the installed passport
    const double baseWasteMW = e.nominalElectricalPowerMW * (1.0 - e.nominalEfficiency01);
    e.engineTypeId = p.id;
    e.engineDisplayName = p.name;
    e.manufacturerName = p.manufacturer;
    e.nominalThrustNewtons = p.number("thrust_mn") * 1.0e6 * units;
    e.nominalElectricalPowerMW = p.number("input_mw") * units;
    e.nominalEfficiency01 = p.number("eff", 0.94);
    // same pad-cooling margin as the FC-RCS: emitter area follows the waste heat
    const double wasteMW = e.nominalElectricalPowerMW * (1.0 - e.nominalEfficiency01);
    e.thermalEmitter.outerDiameterM *= std::sqrt(std::max(0.01, wasteMW / baseWasteMW));
    e.thermalCapacityMJperK *= std::max(0.25, wasteMW / baseWasteMW);
    e.overhaulEquivalentHours = p.overhaulHours > 0.0 ? p.overhaulHours : e.overhaulEquivalentHours;
    e.endOfLifeEquivalentHours = p.lifeHours > 0.0 ? p.lifeHours : e.endOfLifeEquivalentHours;
    return e;
}

[[nodiscard]] inline game::equipment::reactor::EngineReactorDescriptor reactorFrom(const ShipDesignProduct& p, const ShipDesign& s)
{
    if (p.text("runtime_preset") == "elite.reactor.cobra.cobretty.er5.v1")
        return game::equipment::reactor::presets::CobraCobrettyER5PropulsionReactor();

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

[[nodiscard]] inline glm::vec3 toVec3(const glm::dvec3& v)
{
    return {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
}

[[nodiscard]] inline ModuleDescriptor::Placement placementOf(const glm::dvec3& center, const glm::dvec3& size, double massKg)
{
    ModuleDescriptor::Placement p;
    p.valid = true;
    p.centerMeters = toVec3(center);
    p.halfSizeMeters = toVec3(glm::max(size * 0.5, glm::dvec3(0.05)));
    p.massKg = massKg;
    return p;
}

// Rigid-body mass properties of solid boxes (canonical body frame).
struct MassSum
{
    double massKg = 0.0;
    glm::dvec3 moment {0.0};
    struct Box { double m; glm::dvec3 c; glm::dvec3 h; };
    std::vector<Box> boxes;

    void add(double m, const glm::dvec3& c, const glm::dvec3& halfSize)
    {
        if (!(m > 0.0))
            return;
        massKg += m;
        moment += m * c;
        boxes.push_back({m, c, halfSize});
    }
    [[nodiscard]] glm::dvec3 centre() const { return massKg > 0.0 ? moment / massKg : glm::dvec3(0.0); }
    // Principal diagonal about the centre (x = pitch, y = yaw, z = roll).
    [[nodiscard]] glm::dvec3 inertiaDiagonal() const
    {
        const glm::dvec3 g = centre();
        glm::dvec3 I {0.0};
        for (const auto& b : boxes)
        {
            const glm::dvec3 s = b.h * 2.0;
            const glm::dvec3 d = b.c - g;
            I.x += b.m / 12.0 * (s.y * s.y + s.z * s.z) + b.m * (d.y * d.y + d.z * d.z);
            I.y += b.m / 12.0 * (s.x * s.x + s.z * s.z) + b.m * (d.x * d.x + d.z * d.z);
            I.z += b.m / 12.0 * (s.x * s.x + s.y * s.y) + b.m * (d.x * d.x + d.y * d.y);
        }
        return I;
    }
};
} // namespace detail

// Builds the descriptor; error (non-empty) explains why a design cannot be realised.
[[nodiscard]] inline std::optional<ShipDescriptor> buildGenericShipDescriptor(const ShipDesign& s, std::string& error)
{
    using game::ship::plant::EngineSlotRole;
    const auto& catalog = ShipDesignCatalog::instance();
    const auto model = parsePhysicalControlModel(s.physicalModel);
    if (!model)
    {
        error = "unknown physical model " + s.physicalModel;
        return std::nullopt;
    }
    const bool assisted = game::ship::plant::familyOf(*model) == game::ship::plant::PhysicalControlFamily::Assisted;
    const auto* driveItem = s.firstOf("drive");
    const auto* reactorItem = s.firstOf("reactor");
    if (!driveItem || !reactorItem || !s.firstOf("service_reactor") || !s.firstOf("coolant_loop"))
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
    for (const auto& item : s.installed)
    {
        if (!catalog.findProduct(item.productId))
        {
            error = "unknown product " + item.productId;
            return std::nullopt;
        }
        if (item.positions.empty() || item.blocks.size() != item.positions.size())
        {
            error = item.code + ": passport has no positions / primitive blocks";
            return std::nullopt;
        }
    }

    const double L = s.envelopeMeters.x, W = s.envelopeMeters.y, H = s.envelopeMeters.z;
    const glm::dvec3 half {W * 0.5, H * 0.5, L * 0.5};

    ShipDescriptor d;
    d.typeId = ObjectType::None;                 // identity is the passport id (catalog.passportId), not a legacy enum
    d.identity = {s.designation, s.name};
    d.catalog.passportId = s.id;
    d.physicalControlModel = *model;
    d.localFlightControlLaw = game::ship::plant::nativeFlightLaw(*model);
    d.hud.name = s.designation;

    const auto schematicNode = [&](const std::string& code) {
        for (const auto& n : s.schematicNodes)
            if (n.code == code)
                return std::make_pair(n.repairLevel, n.lineReplaceable);
        return std::make_pair(std::string("station"), true);
    };
    const auto isSpof = [&](const std::string& code) {
        return std::find(s.singlePointsOfFailure.begin(), s.singlePointsOfFailure.end(), code) != s.singlePointsOfFailure.end();
    };
    detail::MassSum mass;

    // ---- equipment modules first (their mass fixes the hull structure share)
    std::vector<ModuleDescriptor> equipment;
    std::map<std::string, std::string> driveBySlot, reactorBySlot, nozzleByNode;   // slot / node -> module id
    std::map<std::string, glm::dvec3> driveSlotPos, reactorSlotPos, rcsNodePos;    // normalized mount positions
    std::map<std::string, std::string> rcsNodeParent;                               // node -> first cluster unit module
    std::map<std::string, std::vector<std::string>> nozzleUnitsByNode;               // bank nozzle -> every unit nozzle module
    std::vector<ShipPrimitiveBlock> rcsBlocks;                                       // one box per cluster unit
    const ShipDesignInstalledItem* rcsItem = nullptr;
    double equipmentKg = 0.0;
    for (const auto& item : s.installed)
    {
        const auto& p = *catalog.findProduct(item.productId);
        const auto [repairLevel, lineReplaceable] = schematicNode(item.code);
        const auto makeModule = [&, repairLevel = repairLevel, lineReplaceable = lineReplaceable](
                                    std::string id, const glm::dvec3& center, const glm::dvec3& size, double kg) {
            ModuleDescriptor m;
            m.moduleId = std::move(id);
            m.parentModuleId = detail::sectionOf(center.z, L);
            m.supportModuleIds = {m.parentModuleId};
            m.subsystemId = item.category;
            m.moduleClass = item.productId;
            m.providedReplacementTags = {item.productId};
            m.acceptedReplacementTags = {item.productId};
            m.structuralKind = ModuleStructuralKind::Equipment;
            m.attachmentType = ModuleAttachmentType::Internal;
            m.destructible = true;
            m.isCritical = isSpof(item.code);
            m.hitPriority = 1;
            m.layerIndex = 1;
            m.destroyPolicy = ModuleDestroyPolicy::DisableOnly;
            detail::applyRepairDoctrine(m, repairLevel, lineReplaceable);
            m.equipmentFailure.failureAtDamage01 = 0.95;
            m.maintenance.maintenanceWearThreshold01 = 0.20;
            m.placement = detail::placementOf(center, size, kg);
            return m;
        };

        if (item.category == "rcs")
        {
            rcsItem = &item;
            // per node: nozzle units (24 FC-RCS: one module per nozzle) or k cluster
            // units, EACH its own damageable module with its own three nozzles; a
            // bank nozzle's authority is the sum of its node's living units.
            const bool nozzleUnits = p.text("runtime_preset") == "cobra.rcs.electric.fixed.v1" || item.quantity == 24;
            const int nodes = static_cast<int>(item.positions.size());
            const int unitsPerNode = nozzleUnits ? 1 : std::max(1, item.quantity / std::max(1, nodes));
            for (int n = 0; n < nodes; ++n)
            {
                const std::string node = n < static_cast<int>(item.slots.size()) ? item.slots[n] : ("rcs_node_" + std::to_string(n));
                const auto& block = item.blocks[n];
                rcsNodePos[node] = item.positions[n];
                const glm::dvec3 unitSize = nozzleUnits ? block.sizeMeters : block.sizeMeters / std::cbrt(static_cast<double>(unitsPerNode));
                for (int j = 0; j < unitsPerNode; ++j)
                {
                    // co-located units sit side by side along the hull axis
                    glm::dvec3 centre = block.centerMeters;
                    centre.z += (j - (unitsPerNode - 1) * 0.5) * unitSize.z;
                    const std::string unitId = item.code + "." + node + (unitsPerNode > 1 ? "." + std::to_string(j + 1) : std::string());
                    std::string parent;
                    if (!nozzleUnits)
                    {
                        auto cluster = makeModule(unitId, centre, unitSize, p.massTonnes * 1000.0);
                        cluster.moduleClass = item.productId;
                        parent = cluster.moduleId;
                        if (j == 0)
                            rcsNodeParent[node] = parent;
                        rcsBlocks.push_back({parent, item.category, detail::toVec3(centre), detail::toVec3(unitSize * 0.5)});
                        mass.add(cluster.placement.massKg, centre, unitSize * 0.5);
                        equipmentKg += cluster.placement.massKg;
                        equipment.push_back(std::move(cluster));
                    }
                    if (assisted)
                        continue;      // Assisted flight has no RCS bank: the blocks are installed hardware only
                    for (const auto& nozzle : game::ship::plant::canonical::newtonianRcsNozzles())
                    {
                        if (nozzle.nodeId != node)
                            continue;
                        const double nozzleKg = nozzleUnits ? p.massTonnes * 1000.0 : 0.0;   // a cluster's mass is its block
                        const std::string nozzleId = unitsPerNode > 1
                            ? unitId + "_" + std::string(1, nozzle.nozzleId.back())
                            : rcsNozzleModuleId(item.code, nozzle.nozzleId);
                        auto unit = makeModule(nozzleId, centre, unitSize * (nozzleUnits ? 0.6 : 0.4), nozzleKg);
                        unit.subsystemId = "propulsion.rcs";
                        unit.moduleClass = nozzleUnits ? item.productId : "manoeuvre_nozzle";
                        unit.attachmentType = ModuleAttachmentType::SurfaceMounted;
                        unit.repairable = true;
                        unit.salvageable = true;
                        if (!parent.empty())
                        {
                            unit.parentModuleId = parent;
                            unit.supportModuleIds = {parent};
                        }
                        unit.engine = detail::nozzleFrom(p, 1.0);
                        if (j == 0)
                            nozzleByNode[std::string(nozzle.nozzleId)] = unit.moduleId;
                        nozzleUnitsByNode[std::string(nozzle.nozzleId)].push_back(unit.moduleId);
                        if (nozzleKg > 0.0)
                        {
                            mass.add(nozzleKg, centre, unitSize * 0.3);
                            equipmentKg += nozzleKg;
                        }
                        equipment.push_back(std::move(unit));
                    }
                }
            }
            continue;
        }

        const int count = moduleCountOf(item);
        const int positions = static_cast<int>(item.positions.size());
        if (count == 1 && positions > 1)
        {
            // a bulk position spanning several blocks: one module, box around them
            glm::dvec3 lo(1e30), hi(-1e30);
            for (const auto& b : item.blocks)
            {
                lo = glm::min(lo, b.centerMeters - b.sizeMeters * 0.5);
                hi = glm::max(hi, b.centerMeters + b.sizeMeters * 0.5);
            }
            const double kg = p.massTonnes * item.quantity * 1000.0;
            auto m = makeModule(unitModuleId(item.code, 0), (lo + hi) * 0.5, hi - lo, kg);
            for (const auto& b : item.blocks)   // mass sits in the real blocks
                mass.add(kg / positions, b.centerMeters, b.sizeMeters * 0.5);
            equipmentKg += kg;
            equipment.push_back(std::move(m));
        }
        else
        {
            // `count` units over `positions` blocks (several units may share a block)
            const double kgPerModule = p.massTonnes * (isBulkCategory(item.category) ? item.quantity : 1) * 1000.0;
            for (int k = 0; k < count; ++k)
            {
                const int at = std::min(positions - 1, k * positions / std::max(1, count));
                const int sharing = std::max(1, (count + positions - 1) / positions);
                const auto& b = item.blocks[at];
                const glm::dvec3 size = b.sizeMeters / std::cbrt(static_cast<double>(sharing));
                auto m = makeModule(unitModuleId(item.code, k), b.centerMeters, size, kgPerModule);
                if (item.category == "drive")
                    m.engine = detail::engineFrom(p, s);
                else if (item.category == "reactor")
                    m.engineReactor = detail::reactorFrom(p, s);
                else if (item.category == "tank" && p.text("fluid") == "argon")
                    m.gasTank = game::equipment::rcs::cobraArgonRcsTank();
                const std::string slot = at < static_cast<int>(item.slots.size()) ? item.slots[at] : std::string();
                if (item.category == "drive" && !slot.empty())
                    driveBySlot[slot] = m.moduleId, driveSlotPos[slot] = item.positions[at];
                else if (item.category == "reactor" && !slot.empty())
                    reactorBySlot[slot] = m.moduleId, reactorSlotPos[slot] = item.positions[at];
                mass.add(kgPerModule, b.centerMeters, size * 0.5);
                equipmentKg += kgPerModule;
                equipment.push_back(std::move(m));
            }
        }
    }

    // ---- hull: root spine + three load-bearing sections carry the structure mass
    const double dryKg = s.dryMassTonnes * 1000.0;
    const double structureKg = dryKg - equipmentKg;
    if (structureKg <= 0.0)
    {
        error = "equipment mass " + std::to_string(equipmentKg / 1000.0) + " t exceeds dry mass";
        return std::nullopt;
    }
    {
        ModuleDescriptor root;
        root.moduleId = HullRootModuleId;
        root.subsystemId = "hull";
        root.moduleClass = "hull.spine";
        root.structuralKind = ModuleStructuralKind::StructuralFrame;
        root.attachmentType = ModuleAttachmentType::Structural;
        root.destructible = true;
        root.isCritical = true;
        root.maxHealth = 400.0f;
        root.repairable = true;
        root.destroysWholeObjectIfCatastrophic = true;
        root.hitPriority = 0;
        const glm::dvec3 spine {W * 0.25, H * 0.25, L};
        root.placement = detail::placementOf(glm::dvec3(0.0), spine, structureKg * 0.25);
        mass.add(root.placement.massKg, glm::dvec3(0.0), spine * 0.5);
        // drawn: the keel; the sections (the full envelope) stay damage / mass volumes only,
        // otherwise the placeholder would hide the installed equipment
        d.primitiveBlocks.push_back({root.moduleId, "hull", root.placement.centerMeters, root.placement.halfSizeMeters});
        d.modules.push_back(std::move(root));
        for (const auto& [id, z] : {std::pair<const char*, double> {"hull.fore", -L / 3.0}, {"hull.mid", 0.0}, {"hull.aft", L / 3.0}})
        {
            ModuleDescriptor sec;
            sec.moduleId = id;
            sec.parentModuleId = HullRootModuleId;
            sec.supportModuleIds = {HullRootModuleId};
            sec.subsystemId = "hull";
            sec.moduleClass = "hull.section";
            sec.structuralKind = ModuleStructuralKind::StructuralFrame;
            sec.attachmentType = ModuleAttachmentType::Structural;
            sec.destructible = true;
            sec.maxHealth = 300.0f;
            sec.repairable = true;
            sec.hitPriority = 0;
            const glm::dvec3 size {W, H, L / 3.0};
            sec.placement = detail::placementOf(glm::dvec3(0.0, 0.0, z), size, structureKg * 0.25);
            mass.add(sec.placement.massKg, glm::dvec3(0.0, 0.0, z), size * 0.5);
            d.modules.push_back(std::move(sec));
        }
    }
    for (auto& m : equipment)
        d.modules.push_back(std::move(m));

    // ---- placeholder blocks: every installed position, tied to its module
    for (const auto& item : s.installed)
        for (std::size_t k = 0; k < item.blocks.size(); ++k)
        {
            std::string moduleId;
            if (item.category == "rcs")
            {
                if (!rcsBlocks.empty())
                    continue;      // cluster units: one box per unit, added below
                moduleId = k < item.slots.size() && nozzleByNode.count(item.slots[k] + "_x") ? nozzleByNode[item.slots[k] + "_x"] : std::string();
            }
            else
            {
                const int count = moduleCountOf(item);
                const int positions = static_cast<int>(item.blocks.size());
                moduleId = unitModuleId(item.code, count == 1 ? 0 : std::min(count - 1, static_cast<int>(k) * count / positions));
            }
            d.primitiveBlocks.push_back({moduleId, item.category, detail::toVec3(item.blocks[k].centerMeters),
                                         detail::toVec3(item.blocks[k].sizeMeters * 0.5)});
        }

    d.primitiveBlocks.insert(d.primitiveBlocks.end(), rcsBlocks.begin(), rcsBlocks.end());

    // the placeholder part ids of each module ("<module>#<k>", CatalogShipAssembly.h):
    // a destroyed / detached module hides its own boxes on the client
    {
        std::map<std::string, int> parts;
        for (const auto& b : d.primitiveBlocks)
            if (!b.moduleId.empty())
                for (auto& m : d.modules)
                    if (m.moduleId == b.moduleId)
                    {
                        m.meshPartIds.push_back(b.moduleId + "#" + std::to_string(parts[b.moduleId]++));
                        break;
                    }
    }

    // ---- loadout: the model's slot structure with this hull's passport coordinates
    ShipLoadoutDefinition loadout;
    loadout.shipPassportId = s.id;
    loadout.id = s.id + ":" + game::ship::plant::physicalControlModelName(*model);
    loadout.legacyModelId = *model;
    loadout.flightLaw = game::ship::plant::nativeFlightLaw(*model);
    loadout.propulsionSlots = game::ship::plant::canonical::propulsionSlots(*model);
    for (auto& slot : loadout.propulsionSlots)
    {
        if (slot.role == EngineSlotRole::ManoeuvreCluster)
        {
            const auto it = rcsNodePos.find(slot.id);
            const auto bound = nozzleByNode.find(std::string(slot.id) + "_x");
            if (it == rcsNodePos.end() || bound == nozzleByNode.end())
            {
                error = std::string("Newtonian RCS node ") + slot.id + " has no installed nozzles";
                return std::nullopt;
            }
            slot.normalizedPositionBody = it->second;
            loadout.propulsionModuleIds.push_back(bound->second);
            continue;
        }
        const auto it = driveBySlot.find(slot.id);
        if (it == driveBySlot.end())
        {
            error = std::string("main slot ") + slot.id + " of " + s.physicalModel + " has no installed drive";
            return std::nullopt;
        }
        slot.normalizedPositionBody = driveSlotPos[slot.id];
        loadout.propulsionModuleIds.push_back(it->second);
    }
    if (driveBySlot.size() != static_cast<std::size_t>(std::count_if(loadout.propulsionSlots.begin(), loadout.propulsionSlots.end(),
                                                                      [](const auto& sl) { return sl.role != EngineSlotRole::ManoeuvreCluster; })))
    {
        error = "drive units (" + std::to_string(driveBySlot.size()) + ") do not fill the " + s.physicalModel + " main slots";
        return std::nullopt;
    }
    const auto& groups = game::ship::plant::canonical::installationSlots(*model).engineReactors;
    if (groups.size() != reactorBySlot.size())
    {
        error = "propulsion reactors (" + std::to_string(reactorBySlot.size()) + ") do not match " + std::to_string(groups.size()) +
                " generator groups";
        return std::nullopt;
    }
    for (const auto& g : groups)
    {
        const auto it = reactorBySlot.find(g.slotId);
        if (it == reactorBySlot.end())
        {
            error = std::string("generator group ") + g.slotId + " has no installed reactor";
            return std::nullopt;
        }
        ShipEngineReactorGroupDefinition group;
        group.slotId = g.slotId;
        group.normalizedPositionBody = reactorSlotPos[g.slotId];
        for (const char* engine : g.poweredEngineSlots)
            group.poweredEngineSlotIds.emplace_back(engine);
        group.reactorModuleId = it->second;
        d.powerThermalInstallation.engineReactorModuleIds.push_back(it->second);
        loadout.engineReactorGroups.push_back(std::move(group));
    }
    if (!assisted)
    {
        if (!rcsItem)
        {
            error = "Newtonian design without an RCS bank";
            return std::nullopt;
        }
        for (const auto& nozzle : game::ship::plant::canonical::newtonianRcsNozzles())
        {
            ShipRcsNozzleMountDefinition mount;
            mount.nozzleId = nozzleByNode[std::string(nozzle.nozzleId)];
            mount.unitNozzleIds = nozzleUnitsByNode[std::string(nozzle.nozzleId)];
            mount.nodeId = std::string(nozzle.nodeId);
            mount.positionBody = rcsNodePos[mount.nodeId];
            mount.forceDirectionBody = nozzle.forceDirectionBody;
            loadout.rcsNozzles.push_back(std::move(mount));
        }
    }
    game::ship::registerLoadoutTopology(loadout);
    d.physicalControl.slotModuleIds = loadout.propulsionModuleIds;
    for (std::size_t i = 0; i < loadout.propulsionSlots.size(); ++i)
    {
        if (loadout.propulsionSlots[i].role == EngineSlotRole::AftMain)
            d.mainPropulsion.aftEngineModuleIds.push_back(loadout.propulsionModuleIds[i]);
        else if (loadout.propulsionSlots[i].role == EngineSlotRole::ForeMain)
            d.mainPropulsion.foreEngineModuleIds.push_back(loadout.propulsionModuleIds[i]);
    }
    d.loadouts.push_back(std::move(loadout));

    // ---- ship devices bound to their modules
    auto& c = d.catalog;
    c.crew = s.crew;
    c.passengers = s.passengers;
    c.fluidsCapacityTonnes = s.fluidsTonnes;
    c.consumablesCapacityTonnes = s.consumablesTonnes;
    const auto idsOf = [&](const ShipDesignInstalledItem& item) {
        std::vector<std::string> ids;
        for (int k = 0; k < moduleCountOf(item); ++k)
            ids.push_back(unitModuleId(item.code, k));
        return ids;
    };
    const auto append = [](std::vector<std::string>& to, const std::vector<std::string>& ids) { to.insert(to.end(), ids.begin(), ids.end()); };
    const ShipDesignProduct* radar = nullptr;
    const ShipDesignProduct* comm = nullptr;
    for (const auto& item : s.installed)
    {
        const auto& p = *catalog.findProduct(item.productId);
        const auto ids = idsOf(item);
        const auto& cat = item.category;
        if (cat == "radar")
        {
            append(c.radarModuleIds, ids);
            if (!radar || p.number("range_km") > radar->number("range_km"))
                radar = &p;
        }
        else if (cat == "comm")
        {
            append(c.receiverModuleIds, ids);
            append(c.transmitterModuleIds, ids);
            if (!comm)
                comm = &p;
        }
        else if (cat == "transponder")
            append(c.transponderModuleIds, ids);
        else if (cat == "life_support")
        {
            append(c.lifeSupportModuleIds, ids);
            append(c.cabinModuleIds, ids);
            c.lifeSupportPowerMW += p.number("power_mw") * item.quantity;
            c.lifeSupportPersons += static_cast<int>(p.number("persons")) * item.quantity;
            c.lifeSupportDays = c.lifeSupportDays == 0 ? static_cast<int>(p.number("days"))
                                                       : std::min(c.lifeSupportDays, static_cast<int>(p.number("days")));
        }
        else if (cat == "passenger_module")
            append(c.cabinModuleIds, ids);
        else if (cat == "computer")
            append(c.computerModuleIds, ids);
        else if (cat == "jump_core")
        {
            append(c.jumpCoreModuleIds, ids);
            c.jumpCoreRatingTonnes += p.number("rating_t");
            c.jumpCoreSealed = c.jumpCoreSealed || (p.params.contains("sealed") && p.params["sealed"].is_boolean() && p.params["sealed"].get<bool>());
        }
        else if (cat == "coolant_loop")
            append(c.coolantLoopModuleIds, ids);
        else if (cat == "service_reactor")
            append(c.serviceReactorModuleIds, ids);
        else if (cat == "docking_port" || cat == "boat_bay" || cat == "drone_bay" || cat == "hangar")
            append(c.dockModuleIds, ids);
        else if (cat == "tank")
        {
            append(c.tankModuleIds, ids);
            c.tankCapacityTonnes.push_back(p.number("capacity_t") * item.quantity);
        }
        if (cat == "container_rack" || cat == "train_module" || cat == "bulk_hold" || cat == "cryo_tank")
        {
            const double cap = cat == "container_rack" ? 20.0 * item.quantity
                                                       : p.number("payload_t", p.number("capacity_t")) * item.quantity;
            for (const auto& id : ids)
            {
                c.cargoHolderModuleIds.push_back(id);
                c.cargoHolderCapacityTonnes.push_back(cap / static_cast<double>(ids.size()));
            }
        }
    }
    // The passport payload is the structural limit: holders share it by their
    // own capacity (racks / holds / cars); with none, the payload rides in the
    // mid section.
    {
        double holders = 0.0;
        for (const double cap : c.cargoHolderCapacityTonnes)
            holders += cap;
        if (holders > 0.0)
            for (double& cap : c.cargoHolderCapacityTonnes)
                cap *= s.cargoTonnes / holders;
        else if (s.cargoTonnes > 0.0)
        {
            c.cargoHolderModuleIds.push_back("hull.mid");
            c.cargoHolderCapacityTonnes.push_back(s.cargoTonnes);
        }
    }
    d.powerThermalInstallation.shipReactorModuleId = c.serviceReactorModuleIds.front();
    d.powerThermalInstallation.commonCoolingModuleId = c.coolantLoopModuleIds.front();
    if (radar)
    {
        // The passport radar IS the installed sensor: the sensor backend of the
        // current radar pipeline with this product's range and power.
        auto r = game::TEST_IDEAL_RADAR;
        r.maxRange = radar->number("range_km") * 1000.0;
        r.powerConsumption = radar->number("power_mw", r.powerConsumption);
        r.requiredMountSize = 0.0;
        d.defaultEquipment.radar = r;
        c.radarRangeMeters = r.maxRange;
        d.radarMount.maxSupportedMountSize = 1.0;
        d.radarMount.allowedProfiles = {r.type};
        d.radarMount.normalizedPosition = {0.5f, 0.81f};
        d.radarMount.normalizedSize = {0.35f, 0.21f};
    }
    if (comm)
    {
        auto rx = game::STANDARD_RECEIVER;
        rx.powerConsumption = comm->number("power_mw", rx.powerConsumption);
        d.defaultEquipment.receiver = rx;
        auto tx = ::WSDR_TX13;
        tx.powerConsumption = comm->number("power_mw", tx.powerConsumption);
        d.defaultEquipment.transmitter = tx;
    }

    // ---- rigid body from the placements
    auto& ph = d.physics;
    const double aftThrust = drive->number("thrust_mn") * 1.0e6 * static_cast<double>(d.mainPropulsion.aftEngineModuleIds.size());
    const double foreThrust = drive->number("thrust_mn") * 1.0e6 * static_cast<double>(d.mainPropulsion.foreEngineModuleIds.size());
    ph.massKg = mass.massKg;
    ph.centerOfMassLocalMeters = mass.centre();
    const glm::dvec3 inertia = mass.inertiaDiagonal();
    ph.pitchInertiaKgM2 = inertia.x;
    ph.yawInertiaKgM2 = inertia.y;
    ph.rollInertiaKgM2 = inertia.z;
    ph.physicalActuatorHalfExtentsMeters = half;
    ph.forwardMainEngineAvailable = aftThrust > 0.0;
    ph.reverseMainEngineAvailable = foreThrust > 0.0;
    ph.forwardMainEngineAccelerationMps2 = static_cast<float>(aftThrust / ph.massKg);
    ph.reverseMainEngineAccelerationMps2 = static_cast<float>(foreThrust / ph.massKg);
    const float gimbal = static_cast<float>(drive->number("gimbal_deg") * detail::Pi / 180.0);
    ph.mainGimbalPitchLimitRad = gimbal;
    ph.mainGimbalYawLimitRad = gimbal;
    ph.mainGimbalRateRadPerSec = gimbal > 0.5f ? 1000.0f : 0.6f;
    ph.maxGs = detail::crewLoadLimitG(s);
    ph.maxLinearGs = ph.maxGs * 1.5f;
    ph.turnRadius = static_cast<float>(std::max(10.0, L));
    // Cobra (22 m) rates scaled by size: big hulls turn slower (Newtonian authority is the RCS bank's own)
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
    double rcsAxisThrust = 0.0;
    for (const auto& m : d.modules)
        if (m.engine && m.subsystemId == "propulsion.rcs")
            rcsAxisThrust += m.engine->nominalThrustNewtons;
    ph.manoeuvreThrusterAccel = static_cast<float>(rcsAxisThrust / 3.0 / ph.massKg);
    ph.manoeuvreThrusterAuthorityExplicit = true;
    ph.strafeAccel = assisted ? static_cast<float>(aftThrust / ph.massKg * std::sin(std::min(gimbal, 1.3f)))
                              : ph.manoeuvreThrusterAccel;
    ph.strafeDamping = 6.0f;
    ph.maxStrafeSpeed = 80.0f;
    {
        const auto& tank = game::equipment::rcs::cobraArgonRcsTank();
        ph.manoeuvreGasUsePerSecond = static_cast<float>(1.0 / tank.fullDrawSeconds);
        ph.manoeuvreGasRechargePerSecond = static_cast<float>(1.0 / tank.fullRecoverySeconds);
        ph.manoeuvreGasRestartFraction = static_cast<float>(tank.restartFraction);
    }

    // ---- ship services: service reactor and radiators
    d.reactor = {s.serviceMWe, s.serviceMWe, 3500.0, 2500.0, 4500.0, 3500.0, 120.0, 0.9960, 0.9965, 0.9955};
    d.cooling.radiatorArea = s.radiatorAreaM2;
    d.cooling.panelCount = std::max(1, static_cast<int>(s.radiatorAreaM2 / 10.0));
    d.cooling.emissivity = s.radiatorEmissivity;
    d.cooling.maxTransferPower = s.coolantCapacityMW;
    d.cooling.nominalPower = s.coolantCapacityMW / 90.0;
    d.cooling.peakPower = s.coolantCapacityMW / 40.0;
    d.cooling.radiatorMaterialId = s.radiatorMaterial;

    // ---- payload, survival, bays, slots
    d.storage.cargoMass = static_cast<int>(std::lround(s.cargoTonnes));
    d.storage.cargoVolume = static_cast<int>(std::lround(s.containerSlots * 38.3 + s.cargoTonnes * 1.4));
    d.storage.containerSlots = s.containerSlots;
    d.storage.containerPayloadTonnes = ::catalog::ships::C20PayloadTonnes;
    d.survival.hasEscapePod = s.countOf("escape_capsule") + s.countOf("lifeboat") > 0;
    d.survival.hasNanokitBay = s.countOf("medbay") > 0;
    for (const auto& item : s.installed)
    {
        const auto& p = *catalog.findProduct(item.productId);
        if (item.category == "drone_bay")
            d.docks.droneSlots += static_cast<int>(p.number("drones")) * item.quantity;
        else if (item.category == "hangar")
            d.docks.fighterSlots += static_cast<int>(p.number("craft_slots")) * item.quantity;
        else if (item.category == "boat_bay")
            d.docks.shuttleSlots += static_cast<int>(p.number("craft_slots")) * item.quantity;
        else if (item.category == "radar")
            d.systems.radarSlots += item.quantity;
        else if (item.category == "weapon")
            d.systems.weaponSlots += item.quantity;
        else if (item.category == "comm")
            d.systems.receiverSlots += item.quantity, d.systems.transmitterSlots += item.quantity;
        else if (item.category == "transponder")
            d.systems.transmitterSlots += 0;    // the transponder is the transmitter's payload, not a second set
    }
    d.systems.reactorSlots = static_cast<int>(d.powerThermalInstallation.engineReactorModuleIds.size());
    d.systems.engineSlots = static_cast<int>(d.mainPropulsion.aftEngineModuleIds.size() + d.mainPropulsion.foreEngineModuleIds.size());

    // ---- geometry (placeholder primitives until a mesh exists)
    d.logicalDimensionsValue.length = static_cast<float>(L);
    d.logicalDimensionsValue.width = static_cast<float>(W);
    d.logicalDimensionsValue.height = static_cast<float>(H);
    d.logicalDimensionsValue.scaleReference = ScaleReference::Length;
    d.logicalDimensionsValue.enabled = true;
    d.meshSizeMeters = glm::vec3(static_cast<float>(W), static_cast<float>(H), static_cast<float>(L));
    d.boundingEllipsoid.radius = glm::vec3(static_cast<float>(W * 0.5), static_cast<float>(H * 0.5), static_cast<float>(L * 0.5));
    d.radarCrossSection = std::max(0.5, W * H / 130.0);
    return d;
}

} // namespace game::ship::descriptors
