#pragma once

#include <string>
#include <vector>

#include "src/game/docking/traffic/DockTrafficControllerApi.h"

// Dispatcher block of the docking status terminal. Pure: text from the
// replicated dispatcher status of THIS ship; the client never derives queue or
// availability itself.
namespace game::presentation
{

struct DockDispatcherTerminalInput
{
    game::docking::traffic::DockTrafficPhase phase = game::docking::traffic::DockTrafficPhase::None;
    std::size_t queuePosition = 0;
    std::size_t queueLength = 0;
    std::size_t departureQueuePosition = 0;
    std::string holdPointId;
    std::string padId;
};

[[nodiscard]] inline std::vector<std::string> dockDispatcherTerminalLines(const DockDispatcherTerminalInput& in)
{
    using game::docking::traffic::DockTrafficPhase;
    const std::string hold = in.holdPointId.empty() ? "WAITING" : in.holdPointId;
    const std::string pad = in.padId.empty() ? "--" : in.padId;
    switch (in.phase)
    {
        case DockTrafficPhase::None:
            return {"STATION CONTROL", "NO DOCKING REQUEST"};
        case DockTrafficPhase::Queued:
            return {"DOCKING REQUEST ACCEPTED",
                    "QUEUE: " + std::to_string(in.queuePosition) + " / " + std::to_string(in.queueLength),
                    "HOLD: WAITING", "PAD: --", "ENTRY: STANDBY"};
        case DockTrafficPhase::HoldAssigned:
            return {"PROCEED TO HOLD", "HOLD: " + hold, "PAD: " + pad, "ENTRY: STANDBY"};
        case DockTrafficPhase::AtHold:
            return {"HOLDING", "HOLD: " + hold, "PAD: " + pad, "ENTRY: STANDBY - AWAIT CLEARANCE"};
        case DockTrafficPhase::EntryCleared:
            return {"ENTRY CLEARED", "HOLD: " + hold, "PAD: " + pad, "ENTRY: CLEARED - FOLLOW AXIS, MATCH ROLL"};
        case DockTrafficPhase::StationControl:
            return {"STATION CONTROL", "PAD: " + pad, "HANDS OFF - STATION LANDING"};
        case DockTrafficPhase::Docked:
            return {"DOCKED", "PAD: " + pad};
        case DockTrafficPhase::DepartureQueued:
            return {"DEPARTURE QUEUED", "PAD: " + pad,
                    "POSITION: " + std::to_string(in.departureQueuePosition), "EXIT: STANDBY"};
        case DockTrafficPhase::DepartureCleared:
            return {"DEPARTURE CLEARED", "PAD: " + pad, "EXIT: CLEARED - FOLLOW AXIS OUT"};
        case DockTrafficPhase::Departed:
            return {"DEPARTED", "CLEAR OF THE CONTROLLED ZONE"};
    }
    return {};
}

[[nodiscard]] inline DockDispatcherTerminalInput dockDispatcherTerminalInput(
    const game::docking::traffic::DockTrafficShipStatus& status)
{
    DockDispatcherTerminalInput in;
    in.phase = status.phase;
    in.queuePosition = status.queuePosition;
    in.queueLength = status.queueLength;
    in.departureQueuePosition = status.departureQueuePosition;
    in.holdPointId = status.holdPointId;
    in.padId = status.padId;
    return in;
}

} // namespace game::presentation
