#pragma once

#include <cstdint>
#include <vector>

#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/game/ship/plant/ShipPhysicalControlModel.h"
#include "src/game/equipment/engine/Engine.h"
#include "src/game/ship/plant/ShipPresetEngineManeuvers.h"
#include "src/world/WorldParams.h"

namespace game::ship::control
{

// Public execution seam between pilot/autopilot commands and the current
// vehicle-motion backend.
//
// Today this gateway delegates to SharedShipPhysics + DynamicMotionSystem.
// Future ShipPhysicalPlant/actuator allocation belongs BEHIND this API.
// Callers must not interpret ShipControlState fields to choose engines,
// Assisted/Newtonian translation laws, RCS or navigation execution paths.
class ShipControlExecutionGateway final
{
public:
    enum class TranslationPath : std::uint8_t
    {
        ManualFlight = 0,
        NavigationPrecision,
        NavigationAssisted,
        NavigationAccelerationDemand
    };

    [[nodiscard]] static TranslationPath selectTranslationPath(
        const ShipControlState& control,
        game::navigation::LocalFlightControlLaw law
    ) noexcept;

    // Attitude/control-state evaluation. Does not propagate orientation.
    static void evaluateAttitudeControl(
        ShipTransform& transform,
        const ShipParams& capabilities,
        const ShipControlState& control,
        const WorldParams& world,
        float dt
    );

    // Compatibility helper for client prediction/presentation: evaluate
    // attitude and propagate orientation using the same backend as authority.
    static void integrateAttitude(
        ShipTransform& transform,
        const ShipParams& capabilities,
        const ShipControlState& control,
        const WorldParams& world,
        float dt
    );

    static void propagateOrientation(
        ShipTransform& transform,
        float dt
    );

    // Evaluate translational actuation for one control cadence. This mutates
    // acceleration/velocity controller state but does not advance position.
    [[nodiscard]] static TranslationPath evaluateTranslationControl(
        ShipTransform& transform,
        const ShipParams& capabilities,
        const ShipControlState& control,
        float dt
    );

    // Physical-control overload used by authoritative ShipCore. The accepted
    // translation law still computes the net force, then the per-slot plant
    // restores the rigid-body r x F consequence that aggregate ShipParams
    // cannot represent.
    [[nodiscard]] static TranslationPath evaluateTranslationControl(
        ShipTransform& transform,
        const ShipParams& capabilities,
        const ShipControlState& control,
        game::ship::plant::PhysicalControlModelId model,
        const std::vector<game::ship::plant::EngineSlotState>& slotState,
        float dt,
        const std::vector<game::equipment::engine::MountedEngine>* mountedEngines =
            nullptr,
        game::ship::plant::AssistedManeuverPrograms* maneuverPrograms = nullptr
    );

    // Advance HubTactical translation from the actuation state produced above.
    // Returns false when the transform is not in a valid HubTactical frame.
    [[nodiscard]] static bool propagateHubTacticalTranslation(
        ShipTransform& transform,
        const ShipParams& capabilities,
        double dt
    );
};

} // namespace game::ship::control
