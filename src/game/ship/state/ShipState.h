#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"

namespace game::ship::state
{

// Immutable construction data still lives in ShipDescriptor/registries.
// This block is the per-ship binding to that definition plus the currently
// selected physical construction. Production ships bind once at spawn;
// diagnostics may deliberately select another fixed I/Y/X construction without
// moving/resetting the ship.
struct ShipConstructionState
{
    const ShipDescriptor* definition = nullptr;
    game::ship::plant::PhysicalControlModelId physicalControlModel =
        game::ship::plant::PhysicalControlModelId::AssistedI;
    game::navigation::LocalFlightControlLaw nativeFlightControlLaw =
        game::navigation::defaultLocalFlightControlLaw();
    std::uint64_t revision = 0;

    [[nodiscard]] bool valid() const noexcept
    {
        return definition != nullptr;
    }

    void bind(const ShipDescriptor& descriptor) noexcept
    {
        definition = &descriptor;
        physicalControlModel = descriptor.physicalControlModel;
        nativeFlightControlLaw =
            game::ship::plant::nativeFlightLaw(physicalControlModel);
        ++revision;
    }

    void selectPhysicalControlModel(
        game::ship::plant::PhysicalControlModelId model
    ) noexcept
    {
        physicalControlModel = model;
        nativeFlightControlLaw =
            game::ship::plant::nativeFlightLaw(physicalControlModel);
        ++revision;
    }
};

enum class ShipActuatorStateSource : std::uint8_t
{
    ModuleRuntime = 0,
    DiagnosticOverride
};

struct ShipActuatorState
{
    ShipActuatorStateSource source =
        ShipActuatorStateSource::ModuleRuntime;
    std::vector<game::ship::plant::EngineSlotState> propulsionSlots;
    std::uint64_t revision = 0;

    void resetHealthy(
        game::ship::plant::PhysicalControlModelId model,
        ShipActuatorStateSource newSource =
            ShipActuatorStateSource::ModuleRuntime
    )
    {
        source = newSource;
        propulsionSlots =
            game::ship::plant::healthySlotState(
                game::ship::plant::physicalControlSlots(model)
            );
        ++revision;
    }

    [[nodiscard]] bool topologyMatches(
        game::ship::plant::PhysicalControlModelId model
    ) const noexcept
    {
        return propulsionSlots.size() ==
            game::ship::plant::physicalControlSlots(model).size();
    }

    void synchronizeFromModules(
        std::vector<game::ship::plant::EngineSlotState> projected
    )
    {
        if (source != ShipActuatorStateSource::ModuleRuntime)
            return;

        const auto same =
            propulsionSlots.size() == projected.size() &&
            std::equal(
                propulsionSlots.begin(),
                propulsionSlots.end(),
                projected.begin(),
                [](const auto& a, const auto& b)
                {
                    return
                        a.operational == b.operational &&
                        a.thrust01 == b.thrust01 &&
                        a.vectoring01 == b.vectoring01;
                }
            );

        if (same)
            return;

        propulsionSlots = std::move(projected);
        ++revision;
    }
};

// Canonical per-instance vehicle state root.
//
// Kinematics continue to live in ShipTransform during the current migration,
// but construction, actuator capability and control input no longer live in
// unrelated owner objects. Player, NPC, bot and test craft all receive this
// exact state type because every Ship owns one ShipCore.
struct ShipState
{
    ShipConstructionState construction;
    ShipActuatorState actuators;
    ShipControlState control;
    std::uint64_t revision = 0;

    void initialize(const ShipDescriptor& descriptor)
    {
        construction.bind(descriptor);
        actuators.resetHealthy(
            construction.physicalControlModel,
            ShipActuatorStateSource::ModuleRuntime
        );
        control = {};
        ++revision;
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return construction.valid() &&
            actuators.topologyMatches(
                construction.physicalControlModel
            );
    }

    // Diagnostic construction changes are state transitions, not alternate
    // Ship subclasses or six caller-side branches. Position/velocity/orientation
    // deliberately remain untouched.
    void selectConstructionForDiagnostic(
        game::ship::plant::PhysicalControlModelId model
    )
    {
        construction.selectPhysicalControlModel(model);
        actuators.resetHealthy(
            model,
            ShipActuatorStateSource::DiagnosticOverride
        );
        ++revision;
    }

    void clearDiagnosticConstructionOverride()
    {
        if (!construction.definition)
            return;

        construction.selectPhysicalControlModel(
            construction.definition->physicalControlModel
        );
        actuators.resetHealthy(
            construction.physicalControlModel,
            ShipActuatorStateSource::ModuleRuntime
        );
        ++revision;
    }
};

} // namespace game::ship::state
