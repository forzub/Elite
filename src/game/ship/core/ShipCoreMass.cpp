// ShipCore: CURRENT mass properties (attached modules + live load) and the
// state of installed devices bound to their modules (catalog passports).

#include "src/game/ship/core/ShipCoreInternal.h"

namespace game::ship::core
{
namespace
{
// Solid-box accumulator: mass, first moment and inertia about the origin.
struct MassAccumulator
{
    double massKg = 0.0;
    glm::dvec3 moment {0.0};
    // second moments about the origin: sum m (y^2 + z^2) etc. plus own box terms
    glm::dvec3 originInertia {0.0};

    void add(double m, const glm::dvec3& c, const glm::dvec3& halfSize)
    {
        if (!(m > 0.0))
            return;
        const glm::dvec3 s = halfSize * 2.0;
        massKg += m;
        moment += m * c;
        originInertia.x += m / 12.0 * (s.y * s.y + s.z * s.z) + m * (c.y * c.y + c.z * c.z);
        originInertia.y += m / 12.0 * (s.x * s.x + s.z * s.z) + m * (c.x * c.x + c.z * c.z);
        originInertia.z += m / 12.0 * (s.x * s.x + s.y * s.y) + m * (c.x * c.x + c.y * c.y);
    }
};

const ModuleDescriptor* findDescriptorModule(const ShipDescriptor& d, const std::string& id)
{
    for (const auto& m : d.modules)
        if (m.moduleId == id)
            return &m;
    return nullptr;
}

// Cargo and C20 boxes aboard for a set of attached holders: the holders carry
// the payload and its C20 places; a detached holder takes its share of both.
struct CargoAboard
{
    double cargoTonnes = 0.0;
    int boxes = 0;
};

template<typename Attached>
CargoAboard cargoAboard(const ShipDescriptor& d, const ShipLoadState& load, const Attached& attached)
{
    const auto& c = d.catalog;
    double total = 0.0, aboard = 0.0;
    for (std::size_t i = 0; i < c.cargoHolderModuleIds.size(); ++i)
    {
        total += c.cargoHolderCapacityTonnes[i];
        if (attached(c.cargoHolderModuleIds[i]))
            aboard += c.cargoHolderCapacityTonnes[i];
    }
    CargoAboard out;
    out.cargoTonnes = c.cargoHolderModuleIds.empty() ? load.cargoTonnes : std::min(load.cargoTonnes, aboard);
    const int slots = d.storage.containerSlots;
    if (slots <= 0)
        return out;
    out.boxes = load.containers >= 0
        ? std::min(load.containers, slots)
        : std::min(slots, static_cast<int>(std::ceil(out.cargoTonnes / std::max(1, d.storage.containerPayloadTonnes))));
    if (total > 0.0)
        out.boxes = std::min(out.boxes, static_cast<int>(std::floor(slots * aboard / total + 1.0e-9)));
    return out;
}
} // namespace

ShipLoadState ShipCore::serviceLoad(const ShipDescriptor& descriptor)
{
    ShipLoadState load;
    if (!descriptor.catalog.valid())
        return load;               // legacy hull: its descriptor mass is its service mass
    load.persons = descriptor.catalog.crew;
    load.fluids01 = 1.0;
    load.consumables01 = 1.0;
    return load;
}

void ShipCore::setLoad(const ShipLoadState& load)
{
    m_load = load;
    m_load.cargoTonnes = std::clamp(load.cargoTonnes, 0.0, static_cast<double>(std::max(0, m_desc ? m_desc->storage.cargoMass : 0)));
    m_load.fluids01 = std::clamp(load.fluids01, 0.0, 1.0);
    m_load.consumables01 = std::clamp(load.consumables01, 0.0, 1.0);
    m_load.persons = std::max(0, load.persons);
    // the hold API and the load are ONE cargo quantity
    m_cargo.mass.value = static_cast<int>(std::lround(m_load.cargoTonnes));
    recomputeMassProperties();
}

void ShipCore::recomputeMassProperties()
{
    const ShipDescriptor* d = m_state.construction.definition ? m_state.construction.definition : m_desc;
    if (!d)
        return;
    const auto& runtime = static_cast<const world::modules::ObjectModuleRuntime&>(m_moduleRuntime);
    const auto attached = [&](const std::string& id) {
        const auto* st = runtime.findModule(id);
        // a destroyed unit is wreckage still aboard; only a detached one has left the ship
        return !st || st->state != world::modules::ObjectModuleRuntimeState::Detached;
    };
    const auto& c = d->catalog;

    MassAccumulator acc;
    bool placed = false;
    for (const auto& m : d->modules)
        if (m.placement.valid && attached(m.moduleId))
        {
            acc.add(m.placement.massKg, glm::dvec3(m.placement.centerMeters), glm::dvec3(m.placement.halfSizeMeters));
            placed = true;
        }
    const double hullKg = placed ? acc.massKg : d->physics.massKg;

    // Loads sit in their holders; a detached holder takes its share away.
    const auto spread = [&](const std::vector<std::string>& ids, const std::vector<double>* capacity, double kg) {
        double total = 0.0;
        for (std::size_t i = 0; i < ids.size(); ++i)
            if (attached(ids[i]))
                total += capacity ? (*capacity)[i] : 1.0;
        if (!(kg > 0.0))
            return;
        if (!(total > 0.0) || !placed)
        {
            acc.add(kg, glm::dvec3(d->physics.centerOfMassLocalMeters), glm::dvec3(0.5));   // legacy hull: at its CoM
            return;
        }
        for (std::size_t i = 0; i < ids.size(); ++i)
        {
            if (!attached(ids[i]))
                continue;
            const auto* m = findDescriptorModule(*d, ids[i]);
            const double share = (capacity ? (*capacity)[i] : 1.0) / total;
            if (m && m->placement.valid)
                acc.add(kg * share, glm::dvec3(m->placement.centerMeters), glm::dvec3(m->placement.halfSizeMeters));
            else
                acc.add(kg * share, glm::dvec3(0.0), glm::dvec3(0.5));
        }
    };
    if (!placed)
        acc.add(d->physics.massKg, d->physics.centerOfMassLocalMeters, glm::dvec3(0.5));

    // cargo and its C20 boxes: only what the attached holders still carry
    const auto cargo = cargoAboard(*d, m_load, attached);
    spread(c.cargoHolderModuleIds, &c.cargoHolderCapacityTonnes,
           (cargo.cargoTonnes + cargo.boxes * c.containerTareTonnes) * 1000.0);
    spread(c.tankModuleIds, &c.tankCapacityTonnes, m_load.fluids01 * c.fluidsCapacityTonnes * 1000.0);
    spread(c.cabinModuleIds, nullptr, m_load.consumables01 * c.consumablesCapacityTonnes * 1000.0 +
                                          m_load.persons * c.personMassTonnes * 1000.0);

    auto& mp = m_state.massProperties;
    const double massKg = std::max(1.0, acc.massKg);
    const glm::dvec3 com = acc.moment / massKg;
    glm::dmat3 inertia(0.0);
    if (placed)
    {
        // parallel-axis theorem back to the live centre of mass
        inertia[0][0] = acc.originInertia.x - massKg * (com.y * com.y + com.z * com.z);
        inertia[1][1] = acc.originInertia.y - massKg * (com.x * com.x + com.z * com.z);
        inertia[2][2] = acc.originInertia.z - massKg * (com.x * com.x + com.y * com.y);
    }
    else
    {
        // legacy hull without placements: its authored tensor, scaled by the live mass
        const double k = massKg / std::max(1.0, d->physics.massKg);
        inertia[0][0] = d->physics.pitchInertiaKgM2 * k;
        inertia[1][1] = d->physics.yawInertiaKgM2 * k;
        inertia[2][2] = d->physics.rollInertiaKgM2 * k;
    }
    (void)hullKg;
    const bool changed = std::abs(mp.massKg - massKg) > 1.0e-6 * massKg ||
                         glm::length(mp.centerOfMassLocalMeters - com) > 1.0e-9 ||
                         std::abs(mp.inertiaTensorLocalKgM2[0][0] - inertia[0][0]) > 1.0e-6 * std::max(1.0, inertia[0][0]) ||
                         std::abs(mp.inertiaTensorLocalKgM2[1][1] - inertia[1][1]) > 1.0e-6 * std::max(1.0, inertia[1][1]) ||
                         std::abs(mp.inertiaTensorLocalKgM2[2][2] - inertia[2][2]) > 1.0e-6 * std::max(1.0, inertia[2][2]);
    m_massModuleRevision = m_moduleRuntime.revision();
    if (!changed)
        return;
    mp.massKg = massKg;
    mp.centerOfMassLocalMeters = com;
    mp.inertiaTensorLocalKgM2 = inertia;
    mp.touch();
}

game::simulation::DetachedFragmentMass ShipCore::detachedModuleMass(const std::string& moduleId) const
{
    game::simulation::DetachedFragmentMass out;
    const ShipDescriptor* d = m_state.construction.definition ? m_state.construction.definition : m_desc;
    if (!d)
        return out;
    const auto& runtime = static_cast<const world::modules::ObjectModuleRuntime&>(m_moduleRuntime);
    const auto attachedNow = [&](const std::string& id) {
        const auto* st = runtime.findModule(id);
        return !st || st->state != world::modules::ObjectModuleRuntimeState::Detached;
    };
    const auto withModule = [&](const std::string& id) { return id == moduleId || attachedNow(id); };
    if (const auto* m = findDescriptorModule(*d, moduleId); m && m->placement.valid)
        out.moduleDryKg = m->placement.massKg;
    const auto before = cargoAboard(*d, m_load, withModule);
    const auto after = cargoAboard(*d, m_load, attachedNow);
    out.cargoKg = std::max(0.0, before.cargoTonnes - after.cargoTonnes) * 1000.0;
    out.containerTareKg = std::max(0, before.boxes - after.boxes) * d->catalog.containerTareTonnes * 1000.0;
    out.totalKg = out.moduleDryKg + out.cargoKg + out.containerTareKg;
    out.known = true;
    return out;
}

bool ShipCore::serviceModule(const std::string& moduleId, bool replaceWithNewUnit)
{
    if (!m_desc)
        return false;
    const bool detached = m_detachedFragmentRuntime.hasFragment(moduleId);
    const bool ok = replaceWithNewUnit ? m_moduleRuntime.replaceModuleWithNewUnit(moduleId)
                                       : m_moduleRuntime.repairModule(moduleId);
    if (!ok)
        return false;
    if (detached && replaceWithNewUnit)
    {
        m_structuralLinkRuntime.restoreLinksTouchingModule(moduleId);
        m_detachedFragmentRuntime.removeFragment(moduleId);   // the old unit stays where it drifted; a new one is fitted
    }
    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent, m_desc->typeId, *m_desc, m_moduleRuntime, m_structuralLinkRuntime, m_assemblyRuntime);
    m_hitVolumesDirty = false;
    recomputeMassProperties();
    refreshDeviceState();
    refreshRuntimeState();
    return true;
}

void ShipCore::adoptInstanceState(const ShipCore& source)
{
    m_moduleRuntime = source.m_moduleRuntime;
    m_structuralLinkRuntime = source.m_structuralLinkRuntime;
    m_load = source.m_load;
    m_cargo = source.m_cargo;
    m_propulsionReactorsOn = source.m_propulsionReactorsOn;
    m_propulsionReactorsStartupRemainingSeconds = source.m_propulsionReactorsStartupRemainingSeconds;
    m_reactor = source.m_reactor;
    m_thermal = source.m_thermal;
    m_propulsionThermal = source.m_propulsionThermal;
    m_cooling = source.m_cooling;
    m_emergencyBridge = source.m_emergencyBridge;
    m_lastModuleProjectionRevision = ~std::uint64_t{0};
    m_massModuleRevision = ~std::uint64_t{0};
    m_cachedCapability = {};
    m_assistedManeuvers = {};
    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent, m_desc->typeId, *m_desc, m_moduleRuntime, m_structuralLinkRuntime, m_assemblyRuntime);
    recomputeMassProperties();
    refreshDeviceState();
    refreshRuntimeState();
}

void ShipCore::refreshDeviceState()
{
    const ShipDescriptor* d = m_desc;
    if (!d || !d->catalog.valid())
    {
        // legacy hull: devices are the installed equipment objects themselves
        m_devices.radar = m_equipment.radar.isOperational();
        m_devices.radarRangeMeters = m_devices.radar ? m_equipment.radar.getDesc().maxRange : 0.0;
        return;
    }
    const auto& runtime = static_cast<const world::modules::ObjectModuleRuntime&>(m_moduleRuntime);
    const auto anyAlive = [&](const std::vector<std::string>& ids) {
        int alive = 0;
        for (const auto& id : ids)
            if (const auto* st = runtime.findModule(id))
                if (st->state == world::modules::ObjectModuleRuntimeState::Attached && st->health > 0.0f)
                    ++alive;
        return alive;
    };
    const auto& c = d->catalog;
    ShipDeviceState s;
    s.radar = anyAlive(c.radarModuleIds) > 0;
    s.receiver = anyAlive(c.receiverModuleIds) > 0;
    s.transmitter = anyAlive(c.transmitterModuleIds) > 0;
    s.transponder = anyAlive(c.transponderModuleIds) > 0;
    s.lifeSupport = anyAlive(c.lifeSupportModuleIds) > 0;
    s.computer = anyAlive(c.computerModuleIds) > 0;
    s.jumpCore = !c.jumpCoreModuleIds.empty() && anyAlive(c.jumpCoreModuleIds) == static_cast<int>(c.jumpCoreModuleIds.size());
    s.jumpCoreRatingTonnes = s.jumpCore ? c.jumpCoreRatingTonnes : 0.0;
    s.jumpMassWithinRating = s.jumpCore && currentMassKg() <= c.jumpCoreRatingTonnes * 1000.0;
    s.dockPortsOperational = anyAlive(c.dockModuleIds);

    // the equipment objects follow their hardware
    if (d->defaultEquipment.radar)
        m_equipment.radar.setEnabled(s.radar);
    s.radarRangeMeters = m_equipment.radar.isOperational() ? m_equipment.radar.getDesc().maxRange : 0.0;
    if (d->defaultEquipment.receiver)
        m_equipment.receiver.setEnabled(s.receiver);
    if (d->defaultEquipment.transmitter)
        m_equipment.transmitter.setEnabled(s.transmitter || s.transponder);
    m_lifeSupport.setMode(s.lifeSupport ? CoreSystem::Mode::Normal : CoreSystem::Mode::Damaged);
    m_devices = s;
}

} // namespace game::ship::core
