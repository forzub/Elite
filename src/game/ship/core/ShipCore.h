#pragma once

#include <vector>
#include <array>
#include <cstdint>

#include <unordered_map>
#include <string>
#include "src/world/modules/ObjectModuleRuntime.h"
#include "src/game/equipment/rcs/RcsGasTankUnit.h"

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/ShipVisualIdentity.h"
#include "src/game/ship/ShipRegistry.h"

#include "src/game/ship/core/ShipTransform.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/plant/ShipPresetEngineManeuvers.h"
#include "src/game/ship/telemetry/ShipControlAuthorityTelemetry.h"
#include "src/game/ship/state/ShipState.h"
#include "src/game/ship/capability/ShipCapabilitySnapshot.h"

#include "src/game/ship/core/ShipInventory.h"
#include "src/game/ship/core/CargoBay.h"

#include "src/game/ship/core/ShipSignalController.h"
#include "src/game/ship/core/ShipSlotsState.h"
#include "src/game/ship/core/ShipRole.h"

#include "src/game/ship/sensors/NpcSignalAwareness.h"

#include "src/game/ship/ShipInitData.h"

#include "src/game/equipment/signalNode/processing/SignalReceiver.h"

#include "src/game/equipment/ShipEquipment.h"

#include "src/world/WorldParams.h"

// #include "src/game/math/MathTransform.h"
// #include "src/game/ship/ShipController.h"
// #include "src/game/network/ClientShipCommand.h"

#include "src/world/types/SignalReceptionResult.h"

#include "ReactorSystem.h"
#include "ThermalSystem.h"
#include "PowerBus.h"
#include "CoolingSystem.h"

#include "src/world/types/RadarContactInput.h"
#include "src/game/equipment/radar/RadarDesc.h"

#include "src/game/simulation/ShipCoreStatus.h"

#include "src/game/ship/core/systems/CoreSystem.h"

#include "game/damage/HitComponent.h"
#include "src/game/ship/damage/ShipDamageHandler.h"

#include "src/world/modules/ObjectAssemblyRuntime.h"
#include "src/world/modules/ObjectStructuralLinkRuntime.h"
#include "src/world/modules/ObjectDetachedFragmentRuntime.h"

#include "src/world/modules/ObjectRepairJobRuntime.h"
#include "src/world/modules/ObjectMissingPartRequest.h"
#include "src/game/simulation/ObjectRepairJobSnapshot.h"
#include "src/world/navigation/NavigationObstacle.h"


namespace game::ship::core
{

class ShipCore
{
public:
    ShipCore() = default;
    void togglePropulsionReactors() noexcept;
    void toggleAssistedFreeRotation() noexcept;
    
    void init(
        ShipRole role,
        const ShipDescriptor& descriptor,
        glm::vec3 position,
        const ShipInitData& initData,
        const glm::mat4& orientation
    );

    
    // // ───────────────
    // // Фазы апдейта
    // // ───────────────
 
    void updatePerception(
        float dt,
        const std::vector<world::RadarContactInput>& radarInputs
    );

    void updateSignals(
        float dt,
        const std::vector<WorldSignal>& worldSignals,
        const std::vector<Planet>& planets,
        const std::vector<InterferenceSource>& interferenceSources,
        EntityId ownerId
    );

    // // ───────────────
    // // Cargo API
    // // ───────────────
    int  freeCargoMass() const;
    int  freeCargoVolume() const;
    bool addCargo(int mass, int volume);
    bool removeCargo(int mass, int volume);

    // // ───────────────
    // // Equipment API
    // // ───────────────
    void initShipSlotsFromDescriptor(const ShipDescriptor& descriptor);
    void installDefaultEquipment(const ShipDescriptor& descriptor);

    template<typename ModuleType, typename DescType>
    bool installEquipment(
        const char* equipmentName,
        QuantitySlot& slot,
        std::vector<ModuleType>& modules,
        const DescType& desc
    );

    template<typename ModuleType, typename DescType>
    bool installEquipment(
        const char* equipmentName,
        QuantitySlot& slot,
        ModuleType& module,
        const DescType& desc
    );
    
    template<typename ModuleType>
    bool removeEquipment(
        const char*     equipmentName,
        QuantitySlot&   slot,
        ModuleType&     module
    );

    // Full compatibility path used by callers that want one combined step.
    void updatePhysics(float dt, const WorldParams& world);

    // Runtime-scaling seam: motion must stay temporally coherent even when
    // expensive internal ship systems are executed at a lower activation
    // cadence. These two calls deliberately separate kinematics/control from
    // reactor/thermal/life-support bookkeeping.
    void updateMotionPhysics(float dt, const WorldParams& world);
    void updateMotionControl(float dt, const WorldParams& world);
    void propagateMotionOrientation(float dt);

    // Translation control and propagation are deliberately separate because
    // server activation may decimate control evaluation while preserving
    // fixed-step kinematics. Both cross the single ship-control gateway.
    void updateTranslationControl(float dt);
    bool propagateHubTacticalTranslation(double dt);

    void updateSystems(float dt);
    bool canInstallRadar(
        const ShipDescriptor& shipDesc,
        const RadarDesc& radarDesc
    ) const;


    std::vector<world::modules::ObjectMissingPartRequest>
    buildMissingPartRequests() const;

    bool startRepairJobForClaimedReplacement(
        const std::string& targetModuleId,
        world::modules::ObjectDetachedFragmentRuntime& sourceRuntime,
        const std::string& sourceModuleId
    );

    world::modules::ObjectDetachedFragmentRuntime& detachedFragmentRuntime()
    {
        return m_detachedFragmentRuntime;
    }

    const world::modules::ObjectDetachedFragmentRuntime& detachedFragmentRuntime() const
    {
        return m_detachedFragmentRuntime;
    }

    void syncDetachedFragmentsFromModuleRuntime();
    void updateDetachedFragments(float dt);


    void damageRadiatorPanel(int index, double amount) {
        m_cooling.damagePanel(index, amount);
    }
        
    void repairAllRadiatorPanels() {
        m_cooling.repairAllPanels();
    }

    const world::modules::ObjectAssemblyRuntime& assemblyRuntime() const { return m_assemblyRuntime; }
    world::modules::ObjectAssemblyRuntime& assemblyRuntime() { return m_assemblyRuntime; }
    void updateAssemblyRuntime(double dt);

    std::vector<game::simulation::ObjectRepairJobSnapshot>
    buildRepairJobSnapshots() const;

    int activeRepairJobCount() const;


    game::ShipCoreStatus getCoreStatus() const;

    std::vector<world::navigation::NavigationObstacle>
    buildNavigationObstaclesForRepair(
        const std::string& ignoredModuleId
    ) const;


    
    // ───────────────
    // Доступ к состоянию
    // ───────────────
    ShipTransform&                              transform()                 { return m_transform; }
    const ShipTransform&                        transform() const           { return m_transform; }

    game::ship::state::ShipState&               state()                     { return m_state; }
    const game::ship::state::ShipState&         state() const               { return m_state; }

    ShipControlState&                           control()                   { return m_state.control; }
    const ShipControlState&                     control() const             { return m_state.control; }

    game::ShipEquipment&                        equipment()                 { return m_equipment; }
    const game::ShipEquipment&                  equipment() const           { return m_equipment; }

    ShipInventory&                              inventory()                 { return m_inventory; }
    CargoBay&                                   cargo()                     { return m_cargo; }

    ShipRole                                    role() const                { return m_role; }
    const ShipVisualIdentity&                    visualIdentity() const      { return m_visual; }
    const ShipRegistry&                          registry() const            { return m_registry; }

    const std::vector<SignalReceptionResult>&   signalResults() const       {return m_signalResults;} 

    const ShipDescriptor&                       desc() const                { return *m_state.construction.definition; }
    const ShipDescriptor&                       descriptor() const           { return *m_state.construction.definition; }

    // Refreshes module-derived actuator state once at the ship-control seam.
    // Diagnostic actuator/construction overrides are never overwritten here.
    void                                        refreshRuntimeState();
    bool                                        selectPhysicalConstructionForDiagnostic(
                                                    game::ship::plant::PhysicalControlModelId model
                                                );
    void                                        clearPhysicalConstructionDiagnosticOverride();

    // Authoritative answer to "what can this ship do right now?". Physical
    // topology and damage stay below this boundary.
    game::ship::capability::ShipCapabilitySnapshot capabilities() const;

    // Transitional projection for the accepted motion backend.
    ShipParams                                   effectivePhysics() const;
    game::ship::telemetry::ShipControlAuthorityTelemetry
        controlAuthorityTelemetry() const;
    // Installed RCS gas tank (null without one) and its usable capacity
    // share (damage, health); the nozzle thrust share its fill allows.
    const game::equipment::rcs::RcsGasTankDescriptor*
        newtonianRcsGasTank(double& capacity01) const;
    double newtonianRcsGasThrustShare01() const;
    const RadarModule&                          radar() const               { return m_equipment.radar; }   

    const ReactorSystem&                        reactor() const               { return m_reactor; }  
                                                                    
    void                                        debugPrintCoreSystems() const; 

    ShipSignalController                        m_signalController;
    NpcSignalAwareness                          m_npcAwareness;    
    
    
    // --------- DAMAGE --------
    game::damage::HitComponent& getHitComponent(){ return m_hitComponent; }
    const game::damage::HitComponent& getHitComponent() const{ return m_hitComponent; }
    void applyDamage(const game::damage::DamageEvent& event);

    bool debugDestroyModuleById(const std::string& moduleId);
    bool debugRestoreModuleById(const std::string& moduleId);
    bool debugResetStructure();

    void exportHitVolumes() const;
    void restoreVolume(int index);


    const world::modules::ObjectModuleRuntime& moduleRuntime() const { return m_moduleRuntime; }
    world::modules::ObjectModuleRuntime& moduleRuntime() { return m_moduleRuntime; }

    void initModuleRuntime();
    void syncDestroyedModulesFromHitVolumes();

    const game::damage::HitComponent& hitComponent() const { return m_hitComponent; }
    game::damage::HitComponent& hitComponent() { return m_hitComponent; }

    bool hitVolumesDirty() const { return m_hitVolumesDirty; }
    void markHitVolumesDirty() { m_hitVolumesDirty = true; }
    void clearHitVolumesDirty() { m_hitVolumesDirty = false; }
    
    bool debugDetachModuleById(const std::string& moduleId);
    bool debugReattachModuleById(const std::string& moduleId);
    bool startRepairJobForModule(const std::string& moduleId);
    void updateRepairJobs(float dt);

    bool debugHangModuleById(const std::string& moduleId);
    bool debugReevaluateStructure();
    bool ejectCockpitCapsule();

    world::modules::ObjectStructuralLinkRuntime& structuralLinkRuntime()
    {
        return m_structuralLinkRuntime;
    }

    const world::modules::ObjectStructuralLinkRuntime& structuralLinkRuntime() const
    {
        return m_structuralLinkRuntime;
    }

    bool debugSetStructuralLinkHealth(
        const std::string& linkId,
        float health,
        bool destroyed
    );

private:

    // ───────────────
    // Идентичность
    // ───────────────
    ShipRole                                    m_role = ShipRole::NPC;
    ShipVisualIdentity                          m_visual;
    ShipRegistry                                m_registry;
    const ShipDescriptor*                       m_desc = nullptr;
    const ShipDescriptor*                       m_nativeDescriptor = nullptr;
    // Shared immutable full-installed Assisted Cobra descriptors. Each ship
    // owns its OWN ObjectModuleRuntime; switching profile never resets it.
    const std::array<ShipDescriptor,6>*           m_cobraHardwareProfiles = nullptr;
    game::ship::plant::PhysicalControlModelId    m_initialPhysicalModel =
        game::ship::plant::PhysicalControlModelId::AssistedI;

    // ───────────────
    // Состояние
    // ───────────────
    ShipTransform                               m_transform;
    game::ship::state::ShipState                m_state;
    // Authoritative Newtonian RCS demands and actual nozzle usage for the
    // next systems tick. The old control law expresses intent only; installed
    // nozzle forces/moments are applied by the 24-unit physical allocator.
    glm::dvec3 m_newtonianRcsAngularDemandRadPerSec2 {0.0};
    std::array<double,24> m_newtonianRcsDelivered01 {};
    double m_newtonianRcsElectricalDemandMW = 0.0;
    // Actual RCS electricity sourced from installed ER-5 groups AFTER
    // main-drive consumption. Indexed exactly like installation.engineReactors.
    // The same samples feed generator heat/age; no second invented reactor.
    std::vector<double> m_newtonianRcsEr5DrawMW {};
    double m_newtonianRcsCompressorElectricalMW = 0.0;
    double m_newtonianRcsNetFlow01PerSecond = 0.0;
    // Server-only trace: angular rates before the nonphysical demand calculation.
    glm::dvec3 m_attitudeRatesAtTickStart {0.0};
    bool m_newtonianRcsPhysical = false;
    bool m_propulsionReactorsOn = true;
    double m_serviceThrottleSaved = 0.0;
    std::uint32_t m_propulsionSwitchRevision = 0;
    double m_propulsionReactorsStartupRemainingSeconds = 0.0;
    // Physical six-axis signed maneuver recipes: generated once for this
    // vessel's installed I/Y/X engine layout, rebuilt on damage/repairs.
    // Not serialized over the network; always derived from real hardware.
    // Server snapshot reads may follow thermal/life updates in the SAME tick.
    // Allow the read-only authority query to refresh this exact executor cache
    // from current installed motors without commanding any engine.
    mutable game::ship::plant::AssistedManeuverPrograms m_assistedManeuvers;

    // Resolved response profile cache. Descriptor/topology/damage may be rich,
    // but motion consumes one compact coefficient set until one of those
    // source revisions changes.
    mutable game::ship::capability::ShipCapabilitySnapshot
        m_cachedCapability;
    mutable std::uint64_t m_cachedConstructionRevision =
        ~std::uint64_t{0};
    mutable std::uint64_t m_cachedActuatorRevision =
        ~std::uint64_t{0};
    mutable std::uint64_t m_cachedMassRevision =
        ~std::uint64_t{0};
    // Signed installed RCS authority sampled with the cached capability:
    // indices strafe, lift, body +Z, pitch, yaw, roll (hull roll sign).
    // Published to clients through ShipControlAuthorityTelemetry.
    mutable bool m_cachedNewtonianRcsAuthorityValid = false;
    mutable std::array<double,6> m_cachedNewtonianRcsPositive {};
    mutable std::array<double,6> m_cachedNewtonianRcsNegative {};
    // ShipController                              m_controller;

    // ───────────────
    // Math
    // ───────────────
    // game::math::MathTransform                   m_mathTransform;


    // ───────────────
    // Сигналы
    // ───────────────
    // RX
    SignalReceiver                              m_signalReceiver;
    std::vector<SignalReceptionResult>          m_signalResults;

    

    // ───────────────
    // Хранилища
    // ───────────────
    CargoBay                                    m_cargo;
    ShipInventory                               m_inventory;
    game::ShipEquipment                         m_equipment;
    ShipSystemState                             m_systems;
    ShipDockState                               m_docks;
    
    
    // ───────────────
    // CoreSystem
    // ───────────────
    ReactorSystem                               m_reactor;  // временный пустой дескриптор
    ThermalSystem                               m_thermal;        // ship service coolant
    ThermalSystem                               m_propulsionThermal; // independent propulsion coolant
    CoolingSystem                               m_cooling;                        // будет инициализирован позже
    PowerBus                                    m_powerBus;                       // будет инициализирован позже

    double                                      m_radarDebugTimer = 0.0;
    
    CoreSystem                           m_lifeSupport{0.01, "lifeSupport", game::equipment::PowerPriority::Critical}; // 5 МВт
    CoreSystem                           m_avionics{0.02, "Avionics", game::equipment::PowerPriority::Critical};
    CoreSystem                           m_radiationShield{53.0, "Radiation Shield", game::equipment::PowerPriority::Combat};

    
    // --------- DAMAGE --------
    game::damage::HitComponent                  m_hitComponent;
    game::ship::ShipDamageHandler               m_damageHandler{&m_hitComponent}; 

    // --------- MODULE RUNTIME --------
    world::modules::ObjectModuleRuntime             m_moduleRuntime;
    std::uint64_t                                    m_lastModuleProjectionRevision =
        ~std::uint64_t{0};
    world::modules::ObjectStructuralLinkRuntime     m_structuralLinkRuntime;
    world::modules::ObjectDetachedFragmentRuntime m_detachedFragmentRuntime;
    world::modules::ObjectRepairJobRuntime m_repairJobRuntime;


    world::modules::ObjectAssemblyRuntime           m_assemblyRuntime;
    bool                                            m_hitVolumesDirty = true;

};


}