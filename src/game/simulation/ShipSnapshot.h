#pragma once
#include "src/game/simulation/ShipRefitSnapshot.h"
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <memory>
#include <variant>

#include "src/scene/EntityID.h"
#include "src/game/ship/core/ShipRole.h"
#include "src/game/ship/ShipRegistry.h"
#include "src/world/types/ObjectType.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/game/ship/plant/ShipPhysicalControlModel.h"
#include "src/game/ship/telemetry/ShipControlAuthorityTelemetry.h"
#include "src/world/types/SignalReceptionResult.h"
#include "src/game/equipment/radar/RadarContact.h"
#include "src/game/damage/DamageEvent.h"
#include "src/game/simulation/ShipCoreStatus.h"
#include "src/game/simulation/ShipSimulationTier.h"
#include "src/game/simulation/ShipReferenceFrameSnapshot.h"
#include "src/game/simulation/NavigationExecutionSnapshot.h"
#include "src/game/diagnostics/HubMotionLab.h"
// #include "src/game/geometry/MeshData.h"

#include "src/game/simulation/ObjectGraphSnapshot.h"
#include "src/game/simulation/ShipCombatSnapshot.h"

namespace game::simulation
{
// This ship's dock dispatcher status (DockTrafficController), replicated so
// the docking terminal shows the server's queue / hold / pad decision. The
// client never derives queue position or availability itself.
struct ShipDockDispatcherSnapshot
{
    bool active = false;
    std::uint8_t phase = 0; // game::docking::traffic::DockTrafficPhase
    std::uint32_t queuePosition = 0;
    std::uint32_t queueLength = 0;
    std::uint32_t departureQueuePosition = 0;
    std::string holdPointId;
    std::string padId;
    bool stationControlled = false;
    // Wait estimate (lane movements ahead x a typical movement) and the
    // geometry the client autopilot / widget needs, in the HUB-LOCAL map
    // frame of the ship (motion.localPositionMeters): the assigned hold, the
    // dock entrance and the axis pointing out of the dock.
    float estimatedWaitSeconds = 0.0f;
    std::uint32_t movementsAhead = 0;
    bool holdValid = false;
    glm::dvec3 holdLocalMeters {0.0};
    glm::dvec3 entryLocalMeters {0.0};
    glm::dvec3 axisOutLocal {0.0, 0.0, -1.0};
};
} // namespace game::simulation

namespace game::simulation
{
// Hub dock traffic ship (DockServiceTraffic actor): identity of the leg and
// the REMAINING planned path the traffic model actually executes, as world
// offsets from the ship's own position in this snapshot (hub-orbit drift
// cancels; maps add them to the interpolated ship position).
struct ShipTrafficSnapshot
{
    bool active = false;
    std::string callsign;
    std::string phase;
    std::string destination;
    std::vector<glm::dvec3> routeOffsetsMeters;
};
} // namespace game::simulation

struct ShipSnapshot
{
    EntityId                                            id;
    ShipInstanceId                                      instanceId = 0;
    ShipRole                                            role;
    ObjectType                                          typeId;
    // Stable identity of a catalog ship (design passport id); empty for a
    // legacy hull, which the client still resolves by typeId.
    std::string                                         catalogShipId;
    game::ship::plant::PhysicalControlModelId            physicalControlModel =
        game::ship::plant::PhysicalControlModelId::AssistedI;
    std::uint64_t                                       acknowledgedControlTick = 0;
    game::diagnostics::HubMotionLabActorKind            motionLabKind = game::diagnostics::HubMotionLabActorKind::None;
    game::simulation::ShipSimulationTier                 simulationTier =
        game::simulation::ShipSimulationTier::MaterializedShip;
    game::simulation::ShipMotionFidelity                  motionFidelity =
        game::simulation::ShipMotionFidelity::FullControl;
    game::simulation::ShipMaterializedFidelity           materializedFidelity =
        game::simulation::ShipMaterializedFidelity::Detailed;
    
    ShipTransform                                       transform;
    game::simulation::ShipReferenceFrameSnapshot         referenceFrame;
    std::variant<
        std::monostate,
        game::simulation::NavigationExecutionSnapshot
    >                                                    navigationExecution;
    std::vector<SignalReceptionResult>                  receptions;
    std::vector<game::RadarContact>                     radarContacts;
    std::vector<game::damage::DamageEvent>              damageEvents;
    
    // std::shared_ptr<game::ship::geometry::MeshData>     mesh;
    
    game::ShipCoreStatus                                shipCoreStatus;
    // Per-tick server-authoritative six-axis installed-motor capability.
    // Must not depend on optional heavy module/structural replication.
    game::ship::telemetry::ShipControlAuthorityTelemetry controlAuthorityTelemetry {};
    game::simulation::ObjectGraphSnapshot                graph;
    game::simulation::ShipDockDispatcherSnapshot          dockDispatcher;
    game::simulation::ShipTrafficSnapshot                 traffic;
    // Owned ships only: ownership + the Repair & Refit preview (F7).
    game::simulation::ShipRefitSnapshot                   refit;
    // Weapons / hits: beams this tick, own missiles in flight, recent impacts.
    game::simulation::ShipCombatSnapshot                  combat;
};