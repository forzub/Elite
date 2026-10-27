#pragma once

#include <type_traits>
#include <utility>

#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/navigation/autopilot/AssistedAutopilot.h"
#include "src/game/navigation/autopilot/NewtonianOptimalController.h"

namespace game::navigation::autopilot
{

// Single runtime selection seam between server NPC guidance and a concrete
// flight-law virtual pilot. Player route autopilots instead fix the law at
// compile time through DedicatedClientRouteAutopilotLaws.h.
class AutopilotLawController final
{
public:
    [[nodiscard]] AutopilotControlCommand update(
        LocalFlightControlLaw law,
        const AutopilotControlRequest& request,
        const ShipParams& params
    ) noexcept
    {
        switch (law)
        {
            case LocalFlightControlLaw::Assisted:
                return assisted_.update(request, params);

            case LocalFlightControlLaw::Newtonian:
            default:
                return newtonian_.update(request, params);
        }
    }

    [[nodiscard]] AutopilotControlTelemetry telemetry(
        LocalFlightControlLaw law,
        const ShipParams& params
    ) const noexcept
    {
        switch (law)
        {
            case LocalFlightControlLaw::Assisted:
                return assisted_.telemetry(params);

            case LocalFlightControlLaw::Newtonian:
            default:
                return newtonian_.telemetry(params);
        }
    }

    void reset() noexcept
    {
        assisted_.reset();
        newtonian_.reset();
    }

private:
    AssistedAutopilot assisted_ {};
    NewtonianOptimalController newtonian_ {};
};

// Compile-time API parity guard.  A concrete pilot cannot silently drift to a
// different request or result contract.
template <typename Pilot>
using PilotUpdateResult = decltype(
    std::declval<Pilot&>().update(
        std::declval<const AutopilotControlRequest&>(),
        std::declval<const ShipParams&>()
    )
);

template <typename Pilot>
using PilotTelemetryResult = decltype(
    std::declval<const Pilot&>().telemetry(
        std::declval<const ShipParams&>()
    )
);

static_assert(
    std::is_same_v<
        PilotUpdateResult<AssistedAutopilot>,
        AutopilotControlCommand
    >,
    "AssistedAutopilot must implement the hard autopilot command API"
);

static_assert(
    std::is_same_v<
        PilotUpdateResult<NewtonianOptimalController>,
        AutopilotControlCommand
    >,
    "NewtonianOptimalController must implement the hard autopilot command API"
);

static_assert(
    std::is_same_v<
        PilotTelemetryResult<AssistedAutopilot>,
        AutopilotControlTelemetry
    >,
    "AssistedAutopilot must implement the hard telemetry API"
);

static_assert(
    std::is_same_v<
        PilotTelemetryResult<NewtonianOptimalController>,
        AutopilotControlTelemetry
    >,
    "NewtonianOptimalController must implement the hard telemetry API"
);

} // namespace game::navigation::autopilot
