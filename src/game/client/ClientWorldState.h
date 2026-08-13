#pragma once

#include <unordered_map>
#include "src/scene/EntityID.h"
#include "src/game/ship/core/ShipControlState.h"
#include <glm/glm.hpp>
#include <deque>

#include "src/game/simulation/SimulationSnapshot.h"
#include "src/game/simulation/ShipReferenceFrameSnapshot.h"
#include "src/game/simulation/HubAttachmentSnapshot.h"
#include "src/game/diagnostics/HubMotionLab.h"
#include "src/game/diagnostics/HubMotionLabTelemetry.h"
#include "src/game/client/ClientSystemMapShipSampler.h"
#include "src/game/client/ClientSystemMapInfrastructureSampler.h"
#include "src/game/client/ClientDetailMapRuntimeSampler.h"

#include "render/HUD/WorldLabel.h"
#include "src/world/WorldParams.h"

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/core/ShipRole.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/game/ship/sensors/ShipSignalPresentation.h"

#include "src/world/types/SignalReceptionResult.h"
#include "src/game/equipment/radar/RadarContact.h"
#include "src/game/simulation/ShipCoreStatus.h"
#include "src/game/damage/DamageEvent.h"

#include "scene/EntityID.h"
#include "src/world/types/ObjectType.h"


#include "src/game/geometry/ObjectAssembly.h"

#include <unordered_set>
#include "src/game/simulation/ObjectModuleSnapshot.h"
#include "src/game/simulation/ObjectDetachedFragmentSnapshot.h"
#include "src/game/simulation/ObjectRepairJobSnapshot.h"

#include "src/game/visual/VisualShip.h"
#include "src/game/visual/VisualDrone.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/world/orbits/OrbitalMotion.h"

struct PendingCommand
{
    std::uint64_t tick = 0;
    ShipControlState control;
};


struct ClientShipState
{
    EntityId                                        id;
    ShipRole                                        role;
    ObjectType                                      typeId;
    game::diagnostics::HubMotionLabActorKind        motionLabKind = game::diagnostics::HubMotionLabActorKind::None;

    ShipTransform                                   transform;      // 🔥 единственный источник sim state
    ShipTransform                                   renderTransform;
    game::simulation::ShipReferenceFrameSnapshot    referenceFrame;
    game::simulation::ShipReferenceFrameSnapshot    renderReferenceFrame;
    const ShipDescriptor*                           descriptor = nullptr;

    ShipSignalPresentation                          signalPresentation;
    std::vector<SignalReceptionResult>              receptions;
    std::vector<game::RadarContact>                 radarContacts;

    const game::ship::geometry::ObjectAssembly*     assembly = nullptr;

    game::ShipCoreStatus                            shipCoreStatus;
    std::vector<game::damage::DamageEvent>           damageEvents;

    std::vector<game::simulation::ObjectModuleSnapshot> modules;
    std::vector<game::simulation::StructuralLinkSnapshot> structuralLinks;
    std::vector<game::simulation::ObjectAssemblyModuleSnapshot> assemblyModules;

    std::vector<game::simulation::ObjectDetachedFragmentSnapshot> detachedFragments;
    std::vector<game::simulation::ObjectRepairJobSnapshot> repairJobs;
    std::unordered_set<std::string>                     hiddenPartIds;
    std::unordered_map<std::string, float> detachedVisualAge;
    std::vector<game::simulation::DebugHitVolumeSnapshot> debugHitVolumes;



};



struct ClientObjectState
{
    EntityId                                        id;
    ObjectType                                      type;
    int                                             systemId = -1;
    world::coordinates::WorldPosition worldPosition;
    // Legacy mirror of worldPosition.
    // Do not use as source of truth.
    glm::vec3 position {0.0f};

    // glm::vec3                                       rotation;
    // glm::vec3                                       renderRotation;
    glm::mat4 orientation {1.0f};
    glm::dvec3 linearVelocityMps {0.0};
    glm::mat4 renderOrientation {1.0f};
    game::simulation::HubAttachmentSnapshot hubAttachment;

    // Replicated instance/navigation facts. They are ordinary world state;
    // map code decides how to present them.
    std::string displayName;
    std::string ownerName;
    bool navigationVisible = false;
    std::string navigationParentBodyId;
    world::orbits::OrbitalMotion orbitalMotion;

    // текущие (для рендера)
    world::coordinates::WorldPosition renderWorldPosition;


    const game::ship::geometry::ObjectAssembly*     assembly = nullptr;

    const IObjectDescriptor*                                        descriptor = nullptr;
    std::vector<game::simulation::ObjectModuleSnapshot>             modules;
    std::vector<game::simulation::StructuralLinkSnapshot> structuralLinks;
    std::vector<game::simulation::ObjectAssemblyModuleSnapshot>     assemblyModules;
    // Presentation/interpolated assembly module state.
    // Used only for rendering rotating modules smoothly.
    std::vector<game::simulation::ObjectAssemblyModuleSnapshot> renderAssemblyModules;
    std::vector<game::simulation::ObjectDetachedFragmentSnapshot> detachedFragments;
    std::unordered_set<std::string>                                 hiddenPartIds;
    std::unordered_map<std::string, float> detachedVisualAge;
    std::vector<game::simulation::DebugHitVolumeSnapshot>           debugHitVolumes;


    void setWorldPosition(
        const world::coordinates::WorldPosition& p
    )
    {
        worldPosition = p;
        position = world::coordinates::legacyFloatMeters(worldPosition);
    }
};


struct ClientHubState
{
    std::string id;
    std::string name;
    std::string owner;
    int systemId = -1;
    std::string parentBodyId;
    world::coordinates::WorldPosition worldPosition;
    glm::dvec3 worldVelocityMps {0.0};
    glm::mat4 orientation {1.0f};
    world::orbits::OrbitalMotion motion;
};



class ClientWorldState
{
public:

    void applySnapshot(const SimulationSnapshot& snapshot);
    void update(
        float dt,
        bool authoritativePlayerRendering,
        double renderServerTimeSeconds
    );

    const std::unordered_map<uint32_t, ClientShipState>& ships() const {return m_ships;}
    const std::unordered_map<uint32_t, ClientObjectState>& objects() const {return m_objects;}
    const std::unordered_map<std::string, ClientHubState>& hubs() const {return m_hubs;}

    double presentationServerTimeSeconds() const noexcept
    {
        return m_presentationServerTimeSeconds;
    }

    const game::diagnostics::HubMotionLabPresentationSample&
    hubMotionLabPresentationSample() const noexcept
    {
        return m_hubMotionLabPresentationSample;
    }

    int playerSystemId() const
    {
        for (const auto& [id, ship] : m_ships)
        {
            (void)id;
            if (ship.role == ShipRole::Player)
                return ship.transform.motion.systemId;
        }

        return -1;
    }

    game::client::SystemMapInfrastructureSampleResult sampleSystemMapInfrastructureAtServerTime(
        int systemId,
        double serverTimeSeconds
    ) const
    {
        return game::client::sampleSystemMapInfrastructureAtServerTime(
            m_snapshotBuffer,
            systemId,
            serverTimeSeconds
        );
    }

    std::vector<game::visual::VisualShip>& visualShips()
    {
        return m_visualShips;
    }

    std::vector<game::visual::VisualDrone>& visualDrones()
    {
        return m_visualDrones;
    }

    const std::vector<game::visual::VisualDrone>& visualDrones() const
    {
        return m_visualDrones;
    }

    void clearVisualDrones()
    {
        m_visualDrones.clear();
    }

    const std::vector<game::visual::VisualShip>& visualShips() const
    {
        return m_visualShips;
    }

    game::client::SystemMapShipSampleResult sampleSystemMapShipsAtServerTime(
        int systemId,
        double serverTimeSeconds
    ) const
    {
        return game::client::sampleSystemMapShipsAtServerTime(
            m_snapshotBuffer,
            systemId,
            serverTimeSeconds
        );
    }

    game::client::DetailMapRuntimeSampleResult sampleDetailMapRuntimeAtServerTime(
        int systemId,
        double serverTimeSeconds
    ) const
    {
        return game::client::sampleDetailMapRuntimeAtServerTime(
            m_snapshotBuffer,
            systemId,
            serverTimeSeconds
        );
    }

    void clearVisualShips()
    {
        m_visualShips.clear();
    }

    void predict(
        EntityId id,
        const ShipControlState& control,
        const WorldParams& world,
        float dt
    );

    // Prepare a presentation-only transform between fixed prediction ticks.
    // This never mutates the fixed predicted state used by reconciliation.
    void prepareLocalPredictedPresentation(
        EntityId id,
        const ShipControlState& control,
        const WorldParams& world,
        float fractionalStepSeconds,
        float fixedStepSeconds
    );

    void clearLocalPredictedPresentation() noexcept;

private:

    std::unordered_map<uint32_t, ClientShipState>   m_ships;
    std::unordered_map<uint32_t, ClientObjectState> m_objects;
    std::unordered_map<std::string, ClientHubState>  m_hubs;
    std::vector<game::visual::VisualShip>           m_visualShips;
    std::vector<game::visual::VisualDrone>          m_visualDrones;
    std::deque<SimulationSnapshot>                  m_snapshotBuffer;
    std::uint64_t                                   m_snapshotTimelineRevision = 0;
    int                                             m_snapshotActiveSystemId = -1;
    ShipSignalPresentation                          signalPresentation;
    double                                          m_presentationServerTimeSeconds = 0.0;
    std::uint64_t                                   m_hubMotionLabFrameIndex = 0;
    game::diagnostics::HubMotionLabPresentationSample m_hubMotionLabPresentationSample;

    bool                                            m_hasLocalPredictedPresentationTarget = false;
    std::uint32_t                                   m_localPredictedPresentationShipId = 0;
    ShipTransform                                   m_localPredictedPresentationTarget;
    float                                           m_localPredictionRemainderSeconds = 0.0f;

    bool                                            m_hasPreviousLabPlayerRenderLocal = false;
    glm::dvec3                                      m_previousLabPlayerRenderLocalMeters {0.0};



};
