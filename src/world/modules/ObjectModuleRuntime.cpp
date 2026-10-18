#include "ObjectModuleRuntime.h"

#include <iostream>

namespace world::modules
{

void ObjectModuleRuntime::init(const std::vector<ModuleDescriptor>& descriptors)
{
    m_modules.clear();
    m_moduleIndexById.clear();

    m_modules.reserve(descriptors.size());

    for (const auto& descMod : descriptors)
    {
        ObjectModuleState state;
        state.moduleId = descMod.moduleId;
        state.parentModuleId = descMod.parentModuleId;
        state.parentIndex = -1;
        state.subsystemId = descMod.subsystemId;
        state.moduleClass = descMod.moduleClass;
        state.providedReplacementTags = descMod.providedReplacementTags;
        state.acceptedReplacementTags = descMod.acceptedReplacementTags;

        state.meshPartIds = descMod.meshPartIds;

        state.destroyPolicy = descMod.destroyPolicy;
        state.detachPolicy = descMod.detachPolicy;
        state.attachmentType = descMod.attachmentType;
        state.structuralKind = descMod.structuralKind;

        state.detachChildrenOnLoss = descMod.detachChildrenOnLoss;
        state.destroyChildrenOnLoss = descMod.destroyChildrenOnLoss;
        state.disableChildrenOnLoss = descMod.disableChildrenOnLoss;

        state.destructible = descMod.destructible;
        state.detachable = descMod.detachable;
        state.repairable = descMod.repairable;
        state.salvageable = descMod.salvageable;

        state.isCritical = descMod.isCritical;
        state.destroysParentIfCatastrophic = descMod.destroysParentIfCatastrophic;
        state.destroysWholeObjectIfCatastrophic = descMod.destroysWholeObjectIfCatastrophic;

        state.maxHealth = descMod.maxHealth;
        state.health = descMod.maxHealth;
        state.equipmentDamage = {};
        state.equipmentDamage.sensitivity =
            descMod.equipmentDamageSensitivity;
        state.equipmentDamage.failure =
            descMod.equipmentFailure;
        if (descMod.engine)
        {
            state.engineOperating.temperatureK =
                descMod.engine->nominalCoolantTemperatureK;
        }
        state.state = ObjectModuleRuntimeState::Attached;

        state.hangable = descMod.hangable;
        state.supportModuleIds = descMod.supportModuleIds;
        state.minSupportsForAttached = descMod.minSupportsForAttached;
        state.minSupportsForStable = descMod.minSupportsForStable;
        state.aliveSupportCount = 0;

        // Явные support-links
        if (!descMod.supportLinks.empty())
        {
            for (const auto& linkDesc : descMod.supportLinks)
            {
                ModuleSupportLinkState link;
                link.linkId = linkDesc.linkId;
                link.supportModuleId = linkDesc.supportModuleId;
                link.maxHealth = linkDesc.maxHealth;
                link.health = linkDesc.maxHealth;
                link.impulseTolerance = linkDesc.impulseTolerance;
                link.loadBearing = linkDesc.loadBearing;
                link.destroyed = false;
                state.supportLinks.push_back(std::move(link));
            }
        }
        else
        {
            // Автогенерация link’ов из supportModuleIds
            for (const auto& supportId : descMod.supportModuleIds)
            {
                ModuleSupportLinkState link;
                link.linkId = descMod.moduleId + "::support::" + supportId;
                link.supportModuleId = supportId;
                link.maxHealth = descMod.autoSupportLinkHealth;
                link.health = descMod.autoSupportLinkHealth;
                link.impulseTolerance = descMod.autoSupportImpulseTolerance;
                link.loadBearing = true;
                link.destroyed = false;
                state.supportLinks.push_back(std::move(link));
            }
        }

        m_moduleIndexById[state.moduleId] = static_cast<int>(m_modules.size());
        m_modules.push_back(std::move(state));
    }

    buildHierarchy();
    reevaluateStructuralStates();
    ++m_revision;

    std::cout << "[ObjectModuleRuntime] initialized modules="
              << m_modules.size() << "\n";
}

void ObjectModuleRuntime::buildHierarchy()
{
    for (auto& m : m_modules)
    {
        m.children.clear();
        m.parentIndex = -1;
    }

    for (int i = 0; i < static_cast<int>(m_modules.size()); ++i)
    {
        const auto& parentId = m_modules[i].parentModuleId;
        if (parentId.empty())
            continue;

        if (parentId == m_modules[i].moduleId)
        {
            std::cout << "[ObjectModuleRuntime] WARNING self-parent moduleId="
                      << m_modules[i].moduleId << "\n";
            continue;
        }

        auto it = m_moduleIndexById.find(parentId);
        if (it == m_moduleIndexById.end())
        {
            std::cout << "[ObjectModuleRuntime] WARNING parent not found for "
                      << m_modules[i].moduleId
                      << " parent=" << parentId << "\n";
            continue;
        }

        m_modules[i].parentIndex = it->second;
        m_modules[it->second].children.push_back(i);
    }

    validateHierarchy();
}

bool ObjectModuleRuntime::validateHierarchy() const
{
    bool ok = true;

    for (int i = 0; i < static_cast<int>(m_modules.size()); ++i)
    {
        int current = i;
        int guard = 0;

        while (current >= 0)
        {
            current = m_modules[current].parentIndex;
            ++guard;

            if (guard > static_cast<int>(m_modules.size()) + 1)
            {
                std::cout << "[ObjectModuleRuntime] ERROR hierarchy cycle detected at module="
                          << m_modules[i].moduleId << "\n";
                ok = false;
                break;
            }
        }
    }

    return ok;
}

int ObjectModuleRuntime::findModuleIndex(const std::string& moduleId) const
{
    auto it = m_moduleIndexById.find(moduleId);
    if (it == m_moduleIndexById.end())
        return -1;

    return it->second;
}

const ObjectModuleState* ObjectModuleRuntime::findModule(const std::string& moduleId) const
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return nullptr;

    return &m_modules[index];
}

ObjectModuleState* ObjectModuleRuntime::findModule(const std::string& moduleId)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return nullptr;

    return &m_modules[index];
}

bool ObjectModuleRuntime::isModuleEffectivelyVisible(const std::string& moduleId) const
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;

    int guard = 0;
    while (index >= 0)
    {
        const auto& mod = m_modules[index];

        if (!mod.isVisibleInHull())
            return false;

        index = mod.parentIndex;
        ++guard;

        if (guard > static_cast<int>(m_modules.size()) + 1)
        {
            std::cout << "[ObjectModuleRuntime] ERROR visibility hierarchy cycle at module="
                      << moduleId << "\n";
            return false;
        }
    }

    return true;
}

bool ObjectModuleRuntime::moduleParticipatesInHits(const std::string& moduleId) const
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;

    int guard = 0;
    while (index >= 0)
    {
        const auto& mod = m_modules[index];

        if (!mod.participatesInHits())
            return false;

        index = mod.parentIndex;
        ++guard;

        if (guard > static_cast<int>(m_modules.size()) + 1)
        {
            std::cout << "[ObjectModuleRuntime] ERROR hit hierarchy cycle at module="
                      << moduleId << "\n";
            return false;
        }
    }

    return true;
}

bool ObjectModuleRuntime::isMeshPartVisible(const std::string& partId) const
{
    for (const auto& mod : m_modules)
    {
        for (const auto& meshId : mod.meshPartIds)
        {
            if (meshId == partId)
                return isModuleEffectivelyVisible(mod.moduleId);
        }
    }

    return true;
}

int ObjectModuleRuntime::countAliveSupports(
    int index,
    const ObjectStructuralLinkRuntime* structuralLinks
) const
{
    if (index < 0 || index >= static_cast<int>(m_modules.size()))
        return 0;

    const auto& module = m_modules[index];

    if (structuralLinks)
    {
        int alive = 0;
        bool foundAnyCentralLinks = false;

        for (const auto& link : structuralLinks->links())
        {
            const bool touchesA = (link.moduleAId == module.moduleId);
            const bool touchesB = (link.moduleBId == module.moduleId);

            if (!touchesA && !touchesB)
                continue;

            foundAnyCentralLinks = true;

            if (!link.loadBearing)
                continue;

            if (!link.isAlive())
                continue;

            const std::string& otherId = touchesA ? link.moduleBId : link.moduleAId;

            int supportIndex = findModuleIndex(otherId);
            if (supportIndex < 0)
                continue;

            const auto& support = m_modules[supportIndex];
            if (support.isAttachedLike())
                ++alive;
        }

        if (foundAnyCentralLinks)
            return alive;
    }



    
    // legacy fallback
    if (!module.supportLinks.empty())
    {
        int alive = 0;

        for (const auto& link : module.supportLinks)
        {
            if (!link.loadBearing)
                continue;

            if (!link.isAlive())
                continue;

            int supportIndex = findModuleIndex(link.supportModuleId);
            if (supportIndex < 0)
                continue;

            const auto& support = m_modules[supportIndex];
            if (support.isAttachedLike())
                ++alive;
        }

        return alive;
    }

    if (module.supportModuleIds.empty())
    {
        if (module.parentIndex >= 0)
        {
            const auto& parent = m_modules[module.parentIndex];
            return parent.isAttachedLike() ? 1 : 0;
        }
        return 0;
    }

    int alive = 0;

    for (const auto& supportId : module.supportModuleIds)
    {
        int supportIndex = findModuleIndex(supportId);
        if (supportIndex < 0)
            continue;

        const auto& support = m_modules[supportIndex];
        if (support.isAttachedLike())
            ++alive;
    }

    return alive;
}

void ObjectModuleRuntime::setModuleHealth(const std::string& moduleId, float health)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return;

    if (m_modules[index].health == health)
        return;

    m_modules[index].health = health;
    ++m_revision;
}

bool ObjectModuleRuntime::addEquipmentDamage(
    const std::string& moduleId,
    game::equipment::EquipmentDamageKind kind,
    double amount01
)
{
    const int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;

    auto& module = m_modules[index];
    if (!game::equipment::addEquipmentDamage(
            module.equipmentDamage.accumulated,
            kind,
            amount01))
    {
        return false;
    }

    ++m_revision;
    return true;
}

bool ObjectModuleRuntime::addEngineImpactShock(
    const std::string& moduleId, double hitDamage01
)
{
    const int index=findModuleIndex(moduleId);
    if(index<0 || !std::isfinite(hitDamage01) || hitDamage01<=0.0)
        return false;
    auto& module=m_modules[index];
    if(module.state!=ObjectModuleRuntimeState::Attached ||
       module.health<=0.0f)
        return false;
    auto& engine=module.engineOperating;
    const double hit=std::clamp(hitDamage01,0.0,1.0);
    // Irreversible structural/functional portion remains in Impact damage.
    const double permanent=hit/3.0;
    const double fieldShock=hit-permanent;
    const bool changed=game::equipment::addEquipmentDamage(
        module.equipmentDamage.accumulated,
        game::equipment::EquipmentDamageKind::Impact,
        permanent
    );
    // The remaining part is a transient field-system malfunction; no
    // destroyed casing or emitter area is magically repaired.
    engine.impactShock01=std::clamp(
        engine.impactShock01+fieldShock,0.0,1.0
    );
    std::uint64_t seed=14695981039346656037ULL;
    for(unsigned char c:moduleId)
    {
        seed^=c;
        seed*=1099511628211ULL;
    }
    seed^=++engine.impactEventSerial*0x9e3779b97f4a7c15ULL;
    seed^=seed>>30u;
    seed*=0xbf58476d1ce4e5b9ULL;
    seed^=seed>>27u;
    seed*=0x94d049bb133111ebULL;
    seed^=seed>>31u;
    // Stable 50% outcome; never re-roll while simulating an impact.
    const bool rebound=(seed & 1ULL)!=0;
    if(!rebound)
        engine.impactShockPermanentOnExpiry01=std::clamp(
            engine.impactShockPermanentOnExpiry01+fieldShock,0.0,1.0
        );
    const double delaySeconds=
        1.0+2.0*static_cast<double>((seed>>11u)&0xffffu)/65535.0;
    engine.impactShockSecondsRemaining=std::max(
        engine.impactShockSecondsRemaining,delaySeconds
    );
    (void)changed;
    ++m_revision;
    return true;
}

bool ObjectModuleRuntime::exposeEngineEmitter(
    const std::string& moduleId,
    const game::equipment::engine::EngineUnitDescriptor& passport,
    const game::physics::materials::SurfaceExposure& hit
)
{
    const int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;
    auto& module = m_modules[index];
    if (module.state != ObjectModuleRuntimeState::Attached ||
        module.health <= 0.0f)
        return false;

    const auto before = module.engineOperating.emitter.surface;
    const auto after = game::equipment::engine::applyEngineEmitterExposure(
        passport, before, hit
    );
    if (after.intactArea01 == before.intactArea01 &&
        after.microcrack01 == before.microcrack01)
        return false;
    module.engineOperating.emitter.surface = after;

    // Every equipment component records the same five causes; these two
    // kinds of hits are stored on the annulus, NOT the main thrust chamber.
    // The area reduction has already been applied above and must not be
    // subtracted a second time by the thermal evaluator.
    const auto cause =
        hit.kind == game::physics::materials::SurfaceImpactKind::Laser
            ? game::equipment::EquipmentDamageKind::Laser
            : game::equipment::EquipmentDamageKind::Impact;
    const double lostArea01 = std::max(
        0.0, before.intactArea01 - after.intactArea01
    );
    const double newCracks01 = std::max(
        0.0, after.microcrack01 - before.microcrack01
    );
    game::equipment::addEquipmentDamage(
        module.engineOperating.emitter.damage, cause,
        std::max(lostArea01, 0.25 * newCracks01)
    );

    ++m_revision; // available physical thrust must be rebuilt
    return true;
}

bool ObjectModuleRuntime::replaceEngineEmitter(
    const std::string& moduleId,
    const game::equipment::engine::EngineUnitDescriptor& passport
)
{
    const int index = findModuleIndex(moduleId);
    if (index < 0 ||
        passport.thermalEmitter.servicePolicy !=
            game::equipment::engine::EngineThermalEmitterDescriptor::
                ServicePolicy::ReplaceOnly ||
        !game::physics::materials::findMaterial(
            passport.thermalEmitter.materialId))
        return false;
    auto& module = m_modules[index];
    if (module.state != ObjectModuleRuntimeState::Attached ||
        module.health <= 0.0f)
        return false;

    // A new annulus, not a repair: none of the old annulus's five damage
    // histories or consumed service hours is carried over. The engine
    // itself retains its temperature, lifetime and other five causes.
    module.engineOperating.emitter.surface = {};
    module.engineOperating.emitter.damage = {};
    module.engineOperating.emitter.equivalentOperatingHours = 0.0;
    ++m_revision;
    return true;
}

bool ObjectModuleRuntime::advanceEngineOperatingLife(
    const std::string& moduleId,
    const game::equipment::engine::EngineUnitDescriptor& passport,
    double deliveredNominalThrust01,
    double elapsedSeconds,
    double coolantTemperatureK,
    double* transferredHeatMW
)
{
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0)
        return false;

    const int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;

    auto& module = m_modules[index];
    const bool installed =
        module.state == ObjectModuleRuntimeState::Attached &&
        module.health > 0.0f;

    const auto effective =
        game::equipment::engine::evaluateEngineUnit(
            passport, module.equipmentDamage, module.engineOperating
        );
    const auto thermalCapBefore =
        game::equipment::engine::safeEmitterThrustFraction01(
            passport, effective, module.engineOperating
        );
    const double wearBefore =
        module.engineOperating.emitter.damage.wear01;

    const auto step =
        game::equipment::engine::advanceEngineOperatingState(
            passport,
            effective,
            module.engineOperating,
            installed ? deliveredNominalThrust01 : 0.0,
            elapsedSeconds,
            coolantTemperatureK
        );
    module.engineOperating = step.next;
    if(transferredHeatMW)
        *transferredHeatMW=installed ? step.coolantHeatMW : 0.0;

    // Resolve cinematic field shock once. All temporary force loss is
    // maintained for its scheduled duration, then either recovered or
    // committed to the SAME permanent Impact channel. No healing laser
    // scars, structural damage, emitter holes or wear.
    bool shockResolved=false;
    auto& shock=module.engineOperating;
    if(shock.impactShockSecondsRemaining>0.0)
    {
        shock.impactShockSecondsRemaining=
            std::max(0.0,shock.impactShockSecondsRemaining-elapsedSeconds);
        if(shock.impactShockSecondsRemaining<=0.0)
        {
            if(shock.impactShockPermanentOnExpiry01>0.0)
                game::equipment::addEquipmentDamage(
                    module.equipmentDamage.accumulated,
                    game::equipment::EquipmentDamageKind::Impact,
                    shock.impactShockPermanentOnExpiry01
                );
            shock.impactShock01=0.0;
            shock.impactShockPermanentOnExpiry01=0.0;
            shockResolved=true;
        }
    }

    // The thermal limiter changes in 1%-point telemetry buckets rather
    // than causing a full capability/network rebuild every simulation tick.
    const auto thermalCapAfter =
        game::equipment::engine::safeEmitterThrustFraction01(
            passport, effective, module.engineOperating
        );
    const auto bucket01 = [](double v)
    {
        return static_cast<int>(std::floor(
            game::equipment::engine::finiteClamp01(v) * 100.0 + 1.0e-7
        ));
    };
    const bool protectionChanged =
        bucket01(thermalCapBefore) != bucket01(thermalCapAfter);
    const bool annulusWearChanged =
        bucket01(wearBefore) !=
            bucket01(module.engineOperating.emitter.damage.wear01);

    // Preserve fractional wear/heat precisely in engine service state, but
    // commit damage in 0.001%-point quanta. No per-frame revision storms,
    // capability rebuilds or network snapshots for 10^-11 damage changes.
    constexpr double DamageQuantum01 = 1.0e-5;
    const auto quantizedIncrement = [&](double& pending, double incoming)
    {
        pending += std::max(0.0, incoming);
        const double quanta =
            std::floor(pending / DamageQuantum01);
        if (quanta < 1.0)
            return 0.0;
        const double committed = quanta * DamageQuantum01;
        pending = std::max(0.0, pending - committed);
        return committed;
    };
    const double wearIncrement = quantizedIncrement(
        module.engineOperating.pendingWearDamage01,
        step.wearDamageIncrement01
    );
    const double thermalIncrement = quantizedIncrement(
        module.engineOperating.pendingThermalDamage01,
        step.thermalDamageIncrement01
    );
    const bool wearChanged =
        game::equipment::addEquipmentDamage(
            module.equipmentDamage.accumulated,
            game::equipment::EquipmentDamageKind::Wear,
            wearIncrement
        );
    const bool thermalChanged =
        game::equipment::addEquipmentDamage(
            module.equipmentDamage.accumulated,
            game::equipment::EquipmentDamageKind::Thermal,
            thermalIncrement
        );
    if (wearChanged || thermalChanged ||
        protectionChanged || annulusWearChanged || shockResolved)
        ++m_revision;

    return wearChanged || thermalChanged ||
        protectionChanged || annulusWearChanged || shockResolved;
}

bool ObjectModuleRuntime::setSupportLinkHealth(
    const std::string& moduleId,
    const std::string& linkId,
    float health,
    bool destroyed
)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;

    auto& module = m_modules[index];

    for (auto& link : module.supportLinks)
    {
        if (link.linkId != linkId)
            continue;

        link.health = health;
        link.destroyed = destroyed || health <= 0.0f;

        if (link.health < 0.0f)
            link.health = 0.0f;

        ++m_revision;
        return true;
    }

    return false;
}

void ObjectModuleRuntime::markModuleDestroyed(const std::string& moduleId, float health)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return;

    m_modules[index].health = health;
    onModuleDestroyed(index);
    ++m_revision;
}

void ObjectModuleRuntime::onModuleDestroyed(int index)
{
    if (index < 0 || index >= static_cast<int>(m_modules.size()))
        return;

    auto& module = m_modules[index];

    if (module.state == ObjectModuleRuntimeState::Destroyed ||
        module.state == ObjectModuleRuntimeState::Detached)
        return;



    std::cout << "[ObjectModuleRuntime] module destroyed: "
              << module.moduleId << "\n";

    switch (module.destroyPolicy)
    {
        case ModuleDestroyPolicy::DisableOnly:
            module.state = ObjectModuleRuntimeState::Disabled;
            break;

        case ModuleDestroyPolicy::Detach:
            module.state = ObjectModuleRuntimeState::Detached;
            break;

        case ModuleDestroyPolicy::Vaporize:
            module.state = ObjectModuleRuntimeState::Destroyed;
            break;

        case ModuleDestroyPolicy::Catastrophic:
            module.state = ObjectModuleRuntimeState::Destroyed;

            if (module.destroysWholeObjectIfCatastrophic)
            {
                setWholeObjectDestroyed();
                return;
            }
            break;
    }

    if (module.destroyChildrenOnLoss)
    {
        for (int child : module.children)
            destroySubtree(child);
    }

    if (module.disableChildrenOnLoss)
    {
        for (int child : module.children)
            disableSubtree(child);
    }

    if (module.detachChildrenOnLoss)
    {
        for (int child : module.children)
            detachSubtree(child);
    }

    reevaluateStructuralStates();
}

void ObjectModuleRuntime::detachSubtree(int index)
{
    if (index < 0 || index >= static_cast<int>(m_modules.size()))
        return;

    auto& module = m_modules[index];

    if (module.state == ObjectModuleRuntimeState::Detached ||
        module.state == ObjectModuleRuntimeState::Destroyed)
        return;

    module.state = ObjectModuleRuntimeState::Detached;

    for (int child : module.children)
        detachSubtree(child);
}

void ObjectModuleRuntime::destroySubtree(int index)
{
    if (index < 0 || index >= static_cast<int>(m_modules.size()))
        return;

    auto& module = m_modules[index];

    if (module.state == ObjectModuleRuntimeState::Destroyed)
        return;

    module.state = ObjectModuleRuntimeState::Destroyed;

    for (int child : module.children)
        destroySubtree(child);
}

void ObjectModuleRuntime::disableSubtree(int index)
{
    if (index < 0 || index >= static_cast<int>(m_modules.size()))
        return;

    auto& module = m_modules[index];

    if (module.state == ObjectModuleRuntimeState::Destroyed ||
        module.state == ObjectModuleRuntimeState::Detached)
        return;

    module.state = ObjectModuleRuntimeState::Disabled;

    for (int child : module.children)
        disableSubtree(child);
}

void ObjectModuleRuntime::restoreSubtree(int index)
{
    if (index < 0 || index >= static_cast<int>(m_modules.size()))
        return;

    auto& module = m_modules[index];
    module.state = ObjectModuleRuntimeState::Attached;
    module.health = module.maxHealth;
    module.equipmentDamage.accumulated = {};
    // Structural repair cannot repair a replace-only annulus. It remains
    // damaged/worn until replaceEngineEmitter is explicitly called.

    for (auto& link : module.supportLinks)
    {
        link.health = link.maxHealth;
        link.destroyed = false;
    }

    for (int child : module.children)
        restoreSubtree(child);
}

void ObjectModuleRuntime::restoreAncestors(int index)
{
    int current = index;
    int guard = 0;

    while (current >= 0)
    {
        auto& module = m_modules[current];
        module.state = ObjectModuleRuntimeState::Attached;
        module.health = module.maxHealth;
        module.equipmentDamage.accumulated = {};
        // Do not silently replace annulus when restoring parent structure.
        const auto oldEmitterSurface = module.engineOperating.emitter.surface;
        const auto oldEmitterDamage = module.engineOperating.emitter.damage;
        const double oldEmitterHours =
            module.engineOperating.emitter.equivalentOperatingHours;
        module.engineOperating = {};
        module.engineOperating.emitter.surface = oldEmitterSurface;
        module.engineOperating.emitter.damage = oldEmitterDamage;
        module.engineOperating.emitter.equivalentOperatingHours =
            oldEmitterHours;

        for (auto& link : module.supportLinks)
        {
            link.health = link.maxHealth;
            link.destroyed = false;
        }

        current = module.parentIndex;
        ++guard;

        if (guard > static_cast<int>(m_modules.size()) + 1)
        {
            std::cout << "[ObjectModuleRuntime] ERROR restoreAncestors hierarchy cycle\n";
            return;
        }
    }
}

void ObjectModuleRuntime::restoreModuleBranch(const std::string& moduleId)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return;

    restoreAncestors(index);
    restoreSubtree(index);

    std::cout << "[ObjectModuleRuntime] restoreModuleBranch moduleId="
              << moduleId << "\n";
    ++m_revision;
}

void ObjectModuleRuntime::setWholeObjectDestroyed()
{
    for (auto& module : m_modules)
    {
        module.state = ObjectModuleRuntimeState::Destroyed;
        module.health = 0.0f;

        for (auto& link : module.supportLinks)
        {
            link.health = 0.0f;
            link.destroyed = true;
        }
    }
    ++m_revision;
}

void ObjectModuleRuntime::applyStructuralStateFromSupports(int index,
    const ObjectStructuralLinkRuntime* structuralLinks)
{
    if (index < 0 || index >= static_cast<int>(m_modules.size()))
        return;

    auto& module = m_modules[index];

    if (module.state == ObjectModuleRuntimeState::Destroyed ||
        module.state == ObjectModuleRuntimeState::Detached)
        return;



// ------------------------------------------------------------
// 1. OUTER PANEL RULE
// ------------------------------------------------------------
// Панель обшивки должна отрываться от смерти авто-швов,
// а не от смерти всех связей вообще.
// Старые supportModuleIds к frame не должны удерживать панель,
// если все реальные panel-to-panel seam уничтожены.
if (structuralLinks &&
    module.detachable &&
    module.structuralKind == ModuleStructuralKind::OuterPanel)
{
    int totalAutoPanelSeams = 0;
    int aliveAutoPanelSeams = 0;

    for (const auto& link : structuralLinks->links())
    {
        if (!link.autoGenerated)
            continue;

        if (link.kind != StructuralLinkKind::PanelSeam)
            continue;

        const bool touches =
            link.moduleAId == module.moduleId ||
            link.moduleBId == module.moduleId;

        if (!touches)
            continue;

        if (!link.loadBearing)
            continue;

        ++totalAutoPanelSeams;

        if (link.isAlive())
            ++aliveAutoPanelSeams;
    }

    if (totalAutoPanelSeams > 0)
    {
        module.aliveSupportCount = aliveAutoPanelSeams;

        if (aliveAutoPanelSeams <= 0)
        {
            module.state = ObjectModuleRuntimeState::Detached;

            std::cout
                << "[ObjectModuleRuntime] OUTER PANEL DETACHED BY DEAD AUTO SEAMS: "
                << module.moduleId
                << " totalAutoPanelSeams=" << totalAutoPanelSeams
                << "\n";

            return;
        }
    }
}

// ------------------------------------------------------------
// 2. GENERIC DETACHABLE RULE
// ------------------------------------------------------------
// Для оборудования, кокпита-капсулы, лазеров и прочего:
// если все реальные несущие связи мертвы — отрываем.
if (structuralLinks && module.detachable)
{
    int totalLoadBearingLinks = 0;
    int aliveLoadBearingLinks = 0;

    for (const auto& link : structuralLinks->links())
    {
        const bool touches =
            link.moduleAId == module.moduleId ||
            link.moduleBId == module.moduleId;

        if (!touches)
            continue;

        if (!link.loadBearing)
            continue;

        ++totalLoadBearingLinks;

        if (link.isAlive())
            ++aliveLoadBearingLinks;
    }

    if (totalLoadBearingLinks > 0)
    {
        module.aliveSupportCount = aliveLoadBearingLinks;

        if (aliveLoadBearingLinks <= 0)
        {
            module.state = ObjectModuleRuntimeState::Detached;

            std::cout
                << "[ObjectModuleRuntime] DETACHED BY DEAD STRUCTURAL LINKS: "
                << module.moduleId
                << " totalLoadBearingLinks=" << totalLoadBearingLinks
                << "\n";

            return;
        }
    }
}






const int aliveSupports = countAliveSupports(index, structuralLinks);
module.aliveSupportCount = aliveSupports;

bool hasAnyStructuralLinks = false;

if (structuralLinks)
{
    for (const auto& link : structuralLinks->links())
    {
        if (link.moduleAId == module.moduleId ||
            link.moduleBId == module.moduleId)
        {
            hasAnyStructuralLinks = true;
            break;
        }
    }
}

const bool hasLegacySupports =
    !module.supportLinks.empty() ||
    !module.supportModuleIds.empty();

const bool hasParentSupport =
    module.parentIndex >= 0 &&
    module.structuralKind != ModuleStructuralKind::OuterPanel;

const bool hasAnySupportSource =
    hasAnyStructuralLinks ||
    hasLegacySupports ||
    hasParentSupport;

if (!hasAnySupportSource)
    return;

// Если у модуля есть реальные structural-links, то 0 живых связей
// не может означать Attached, даже если minSupportsForAttached случайно равен 0.
int requiredSupports = module.minSupportsForAttached;

if (hasAnyStructuralLinks && requiredSupports <= 0)
    requiredSupports = 1;

if (aliveSupports >= requiredSupports)
{
    if (module.state == ObjectModuleRuntimeState::Hanging)
        module.state = ObjectModuleRuntimeState::Attached;
    return;
}

    if (aliveSupports > 0)
    {
        if (module.hangable && aliveSupports < module.minSupportsForAttached)
        {
            module.state = ObjectModuleRuntimeState::Hanging;
            return;
        }

        return;
    }

    if (module.detachable)
    {
        module.state = ObjectModuleRuntimeState::Detached;

        if (module.detachChildrenOnLoss)
        {
            for (int child : module.children)
                detachSubtree(child);
        }
        return;
    }

    switch (module.destroyPolicy)
    {
        case ModuleDestroyPolicy::DisableOnly:
            module.state = ObjectModuleRuntimeState::Disabled;
            break;

        case ModuleDestroyPolicy::Detach:
            module.state = ObjectModuleRuntimeState::Detached;
            break;

        case ModuleDestroyPolicy::Vaporize:
        case ModuleDestroyPolicy::Catastrophic:
            module.state = ObjectModuleRuntimeState::Destroyed;
            break;
    }
}

void ObjectModuleRuntime::reevaluateStructuralStates(
    const ObjectStructuralLinkRuntime* structuralLinks
)
{
    for (int i = 0; i < static_cast<int>(m_modules.size()); ++i)
    {
        applyStructuralStateFromSupports(i, structuralLinks);
    }
    ++m_revision;
}

bool ObjectModuleRuntime::reattachModule(const std::string& moduleId)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;

    auto& module = m_modules[index];

    if (!module.repairable && !module.salvageable)
    {
        std::cout
            << "[ObjectModuleRuntime] reattach rejected, module is not repairable/salvageable: "
            << moduleId << "\n";
        return false;
    }

    module.state = ObjectModuleRuntimeState::Attached;
    module.health = module.maxHealth;

    module.aliveSupportCount = module.minSupportsForAttached;

    for (auto& link : module.supportLinks)
    {
        link.health = link.maxHealth;
        link.destroyed = false;
    }

    std::cout
        << "[ObjectModuleRuntime] module reattached: "
        << moduleId << "\n";

    ++m_revision;
    return true;
}

bool ObjectModuleRuntime::debugForceDetach(const std::string& moduleId)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;

    auto& module = m_modules[index];

    // Каркас, reactor-frame и прочие несъемные structural core
    // нельзя отрывать как панель. Иначе отрывается всё дерево.
    if (!module.detachable)
    {
        std::cout
            << "[ObjectModuleRuntime] debugForceDetach rejected non-detachable module="
            << moduleId << "\n";
        return false;
    }

    if (module.state == ObjectModuleRuntimeState::Destroyed ||
        module.state == ObjectModuleRuntimeState::Detached)
        return false;

    // ВАЖНО: detach только самого модуля.
    // Поддерево отрываем только через реальные политики damage-логики.
    module.state = ObjectModuleRuntimeState::Detached;

    reevaluateStructuralStates();
    ++m_revision;
    return true;
}

bool ObjectModuleRuntime::debugForceHang(const std::string& moduleId)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;

    auto& module = m_modules[index];
    if (module.state == ObjectModuleRuntimeState::Destroyed ||
        module.state == ObjectModuleRuntimeState::Detached)
        return false;

    if (!module.hangable)
        return false;

    module.state = ObjectModuleRuntimeState::Hanging;
    ++m_revision;
    return true;
}

bool ObjectModuleRuntime::debugForceDestroy(const std::string& moduleId)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;

    m_modules[index].health = 0.0f;
    onModuleDestroyed(index);
    ++m_revision;
    return true;
}

} // namespace world::modules