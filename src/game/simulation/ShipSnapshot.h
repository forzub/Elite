#pragma once
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

struct ShipSnapshot
{
    EntityId                                            id;
    ShipInstanceId                                      instanceId = 0;
    ShipRole                                            role;
    ObjectType                                          typeId;
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
};