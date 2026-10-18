#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>

#include "src/world/modules/ObjectModuleState.h"
#include "src/world/descriptors/ModuleDescriptor.h"
#include "src/world/modules/ObjectStructuralLinkRuntime.h"

namespace world::modules
{

class ObjectModuleRuntime
{
public:
    void init(const std::vector<ModuleDescriptor>& descriptors);
    void buildHierarchy();

    int findModuleIndex(const std::string& moduleId) const;
    const ObjectModuleState* findModule(const std::string& moduleId) const;
    ObjectModuleState* findModule(const std::string& moduleId);

    bool isMeshPartVisible(const std::string& partId) const;
    bool isModuleEffectivelyVisible(const std::string& moduleId) const;
    bool moduleParticipatesInHits(const std::string& moduleId) const;

    void markModuleDestroyed(const std::string& moduleId, float health = 0.0f);
    void setModuleHealth(const std::string& moduleId, float health);

    // Adds real accumulated equipment damage and increments revision only when
    // the stored state actually changes.
    bool addEquipmentDamage(
        const std::string& moduleId,
        game::equipment::EquipmentDamageKind kind,
        double amount01
    );

    // Collision/explosion on an ENGINE: persistent 1/3 of hit, reversible
    // field shock 2/3. The outcome is deterministic per module/hit and
    // settles after 1..3 sec; this does not repair mechanical structure.
    bool addEngineImpactShock(
        const std::string& moduleId,
        double hitDamage01
    );

    // Resolve an incident that actually hit a separately identified engine
    // hot annulus. Surface loss is not direct thrust/gimbal damage.
    bool exposeEngineEmitter(
        const std::string& moduleId,
        const game::equipment::engine::EngineUnitDescriptor& passport,
        const game::physics::materials::SurfaceExposure& hit
    );

    // Replace-only consumable: discard the old annulus, preserving engine
    // housing temperature, thrust/TVC history and main-engine wear.
    bool replaceEngineEmitter(
        const std::string& moduleId,
        const game::equipment::engine::EngineUnitDescriptor& passport
    );

    // Advance a real installed engine using physical allocator telemetry.
    // Returns true ONLY if the five-channel functional damage changed.
    // Temperature and service-hours updates do not trigger rebuild of the
    // ship's capability snapshot every frame.
    bool advanceEngineOperatingLife(
        const std::string& moduleId,
        const game::equipment::engine::EngineUnitDescriptor& passport,
        double deliveredNominalThrust01,
        double elapsedSeconds,
        double coolantTemperatureK,
        double* transferredHeatMW = nullptr
    );

    void onModuleDestroyed(int index);

    void detachSubtree(int index);
    void destroySubtree(int index);
    void disableSubtree(int index);

    void restoreModuleBranch(const std::string& moduleId);
    void restoreSubtree(int index);
    void restoreAncestors(int index);

    void setWholeObjectDestroyed();

    const std::vector<ObjectModuleState>& modules() const { return m_modules; }

    // Mutable access conservatively invalidates all derived projections.
    std::vector<ObjectModuleState>& modules()
    {
        ++m_revision;
        return m_modules;
    }

    std::uint64_t revision() const noexcept { return m_revision; }

    void reevaluateStructuralStates(
        const ObjectStructuralLinkRuntime* structuralLinks = nullptr
    );

    bool debugForceDetach(const std::string& moduleId);
    bool reattachModule(const std::string& moduleId);
    bool debugForceHang(const std::string& moduleId);
    bool debugForceDestroy(const std::string& moduleId);

    // Новый API для support-links
    bool setSupportLinkHealth(
        const std::string& moduleId,
        const std::string& linkId,
        float health,
        bool destroyed
    );

private:
    bool validateHierarchy() const;
    int countAliveSupports(
        int index,
        const ObjectStructuralLinkRuntime* structuralLinks = nullptr
    ) const;

    void applyStructuralStateFromSupports(
        int index,
        const ObjectStructuralLinkRuntime* structuralLinks = nullptr
    );

private:
    std::vector<ObjectModuleState>         m_modules;
    std::unordered_map<std::string, int>   m_moduleIndexById;
    std::uint64_t                          m_revision = 0;
};

} // namespace world::modules