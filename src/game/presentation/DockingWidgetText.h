#pragma once

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <vector>

#include "src/game/docking/traffic/DockTrafficControllerApi.h"
#include "src/game/presentation/DockPlannerTerminalTelemetry.h"

// The player's small docking widget (author 2026-11-07: "небольшой виджет с основными
// подсказками - идёт расчёт, движемся, расстояние, очередь в док - какой по очереди, время
// ожидания"). Pure: text from the client's own docking phase and the replicated dispatcher
// status of THIS ship; queue / wait come only from the dispatcher.
namespace game::presentation
{

enum class DockingWidgetClientPhase
{
    Idle = 0,
    Stabilizing,
    Planning,
    RouteReady,
    Executing,
    SecuringCourse
};

struct DockingWidgetInput
{
    bool requestPending = false;      // an automatic docking request is active on the client
    bool padLanding = false;          // the request targets a hub city pad (the planner flies to it): ПОСАДКА
    DockingWidgetClientPhase clientPhase = DockingWidgetClientPhase::Idle;
    bool dispatcherActive = false;
    game::docking::traffic::DockTrafficPhase phase = game::docking::traffic::DockTrafficPhase::None;
    std::size_t queuePosition = 0;
    std::size_t queueLength = 0;
    std::size_t departureQueuePosition = 0;
    std::size_t movementsAhead = 0;
    double estimatedWaitSeconds = 0.0;
    std::string holdPointId;
    std::string padId;
    bool distanceKnown = false;
    double distanceToEntryMeters = 0.0;
    double speedMps = 0.0;
    std::string failure;              // the client planner's failure, if any
    bool stationControlled = false;   // the station flies the ship (capture / departure)
    bool releaseKnown = false;        // departure: the distance to the release point
    double releaseDistanceMeters = 0.0;
    bool beyondGate = false;          // departure: already outside the dock (takeover possible)
    bool justReleased = false;        // the station let the ship go a moment ago
    double takeoverSecondsLeft = -1.0;  // >= 0: the pilot's takeover countdown
};

struct DockingWidgetField
{
    std::string label;
    std::string value;
};

// The panel (author 2026-11-07: "небольшая панель ... без больших расшифровок"):
// a short status chip and a few label / value pairs. The full phrase goes to
// the HUD console (the docking terminal) as `consoleLine`.
struct DockingWidgetText
{
    bool visible = false;
    std::string title;
    std::string chip;
    int chipTone = 0;   // 0 neutral, 1 waiting, 2 cleared / docked, 3 error
    std::vector<DockingWidgetField> fields;
    std::string consoleLine;
    std::vector<std::string> lines;   // = consoleLine (+ the details) for logs / tests
};

// Every visible word comes from the localization catalog (cockpit.dock_widget.* / cockpit.unit.*
// in src/assets/localization/ui/cockpit/flight.json) through `text`; numbers and pad / hold IDs are data.
using DockingWidgetLookup = TerminalLocalizedText::Lookup;

[[nodiscard]] inline std::string dockingWidgetDistance(const DockingWidgetLookup& text, double meters)
{
    if (meters >= 1000.0)
        return TerminalLocalizedText::render(text, "cockpit.unit.distance_km", {TerminalLocalizedText::number(meters / 1000.0, 1)});
    return TerminalLocalizedText::render(text, "cockpit.unit.distance_m", {TerminalLocalizedText::number(meters, 0)});
}

[[nodiscard]] inline std::string dockingWidgetWait(const DockingWidgetLookup& text, double seconds)
{
    if (seconds < 60.0)
        return "~" + TerminalLocalizedText::render(text, "cockpit.unit.time_s", {TerminalLocalizedText::number(std::max(0.0, seconds), 0)});
    return "~" + TerminalLocalizedText::render(text, "cockpit.unit.time_min", {TerminalLocalizedText::number(std::ceil(seconds / 60.0), 0)});
}

[[nodiscard]] inline DockingWidgetText dockingWidgetText(const DockingWidgetInput& in, const DockingWidgetLookup& text)
{
    using game::docking::traffic::DockTrafficPhase;
    const auto tr = [&text](const char* key, std::initializer_list<std::string> args = {}) {
        return TerminalLocalizedText::render(text, key, args);
    };
    const auto speed = [&]() {
        return tr("cockpit.unit.speed_mps", {TerminalLocalizedText::number(in.speedMps, 0)});
    };
    DockingWidgetText out;
    // A departing ship stays on the panel until the station lets it go (its
    // dispatcher record is already "departed" at the lane clear point).
    // Released on the departure (its lane may still be held until the clear
    // point): no longer the station's ship - only the short "yours" notice.
    const bool releasedOnDeparture = !in.stationControlled &&
        (in.phase == DockTrafficPhase::DepartureCleared || in.phase == DockTrafficPhase::Departed);
    const bool dispatcher = in.dispatcherActive && !releasedOnDeparture &&
        ((in.phase != DockTrafficPhase::None && in.phase != DockTrafficPhase::Departed) || in.stationControlled);
    const bool departing = in.stationControlled &&
        (in.phase == DockTrafficPhase::DepartureCleared || in.phase == DockTrafficPhase::Departed);
    if (!in.requestPending && !dispatcher && !in.justReleased)
        return out;
    out.visible = true;
    out.title = tr(departing || (in.justReleased && !dispatcher) ? "cockpit.dock_widget.title_departure"
                   : in.padLanding                                 ? "cockpit.dock_widget.title_pad_landing"
                                                                   : "cockpit.dock_widget.title_docking");
    if (in.justReleased && !dispatcher && !in.requestPending)
    {
        out.chip = tr("cockpit.dock_widget.chip_your_control");
        out.chipTone = 2;
        out.consoleLine = tr("cockpit.dock_widget.console_released");
        out.lines.push_back(out.consoleLine);
        out.fields.push_back({tr("cockpit.dock_widget.field_speed"), speed()});
        return out;
    }

    const auto clientStatus = [&]() -> std::string {
        switch (in.clientPhase)
        {
            case DockingWidgetClientPhase::Stabilizing: return tr("cockpit.dock_widget.status_stabilizing");
            case DockingWidgetClientPhase::Planning:
            case DockingWidgetClientPhase::RouteReady: return tr("cockpit.dock_widget.status_planning");
            case DockingWidgetClientPhase::Executing: return tr("cockpit.dock_widget.status_executing");
            case DockingWidgetClientPhase::SecuringCourse: return tr("cockpit.dock_widget.status_securing");
            default: return tr("cockpit.dock_widget.status_request");
        }
    };
    std::string status;
    bool waiting = false;
    bool showDistance = true;
    switch (dispatcher ? in.phase : DockTrafficPhase::None)
    {
        case DockTrafficPhase::None: status = clientStatus(); break;
        case DockTrafficPhase::Queued:
            status = tr("cockpit.dock_widget.status_queued");
            waiting = true;
            break;
        case DockTrafficPhase::HoldAssigned:
            status = tr("cockpit.dock_widget.status_hold_assigned", {in.holdPointId});
            waiting = true;
            break;
        case DockTrafficPhase::AtHold:
            status = tr("cockpit.dock_widget.status_at_hold", {in.holdPointId});
            waiting = true;
            break;
        case DockTrafficPhase::EntryCleared:
            status = tr("cockpit.dock_widget.status_entry_cleared", {clientStatus()});
            break;
        case DockTrafficPhase::StationControl:
            status = tr("cockpit.dock_widget.status_station_control", {in.padId});
            showDistance = false;
            break;
        case DockTrafficPhase::Docked:
            status = tr("cockpit.dock_widget.status_docked", {in.padId});
            showDistance = false;
            break;
        case DockTrafficPhase::DepartureQueued:
            status = tr("cockpit.dock_widget.status_departure_queued", {std::to_string(in.departureQueuePosition)});
            waiting = true;
            showDistance = false;
            break;
        case DockTrafficPhase::DepartureCleared:
        case DockTrafficPhase::Departed:
            // no changing numbers here: the console prints every new phrase
            status = tr(in.beyondGate ? "cockpit.dock_widget.status_beyond_gate" : "cockpit.dock_widget.status_station_departing");
            showDistance = false;
            break;
        default: break;
    }
    if (!in.failure.empty())
        status = tr("cockpit.dock_widget.status_error", {in.failure});
    out.lines.push_back(status);

    if (showDistance && in.distanceKnown)
        out.lines.push_back(tr("cockpit.dock_widget.line_distance", {dockingWidgetDistance(text, in.distanceToEntryMeters), speed()}));
    if (waiting)
    {
        std::string q;
        if (in.phase == DockTrafficPhase::Queued && in.queuePosition > 0)
            q = tr("cockpit.dock_widget.line_queue", {std::to_string(in.queuePosition), std::to_string(in.queueLength)}) + "  ·  ";
        q += tr("cockpit.dock_widget.line_ahead", {std::to_string(in.movementsAhead), dockingWidgetWait(text, in.estimatedWaitSeconds)});
        out.lines.push_back(q);
    }
    else if (dispatcher && !in.padId.empty() &&
             (in.phase == DockTrafficPhase::EntryCleared))
        out.lines.push_back(tr("cockpit.dock_widget.line_pad", {in.padId}));

    out.consoleLine = tr(departing ? "cockpit.dock_widget.console_departure" : "cockpit.dock_widget.console_docking", {status});
    // ---- the panel ----
    const DockTrafficPhase ph = dispatcher ? in.phase : DockTrafficPhase::None;
    switch (ph)
    {
        case DockTrafficPhase::None:
            out.chip = tr(in.clientPhase == DockingWidgetClientPhase::Planning || in.clientPhase == DockingWidgetClientPhase::RouteReady
                ? "cockpit.dock_widget.chip_calculating"
                : in.clientPhase == DockingWidgetClientPhase::Executing ? "cockpit.dock_widget.chip_en_route" : "cockpit.dock_widget.chip_request");
            break;
        case DockTrafficPhase::Queued: out.chip = tr("cockpit.dock_widget.chip_queue"); out.chipTone = 1; break;
        case DockTrafficPhase::HoldAssigned: out.chip = tr("cockpit.dock_widget.chip_to_point", {in.holdPointId}); out.chipTone = 1; break;
        case DockTrafficPhase::AtHold: out.chip = tr("cockpit.dock_widget.chip_holding", {in.holdPointId}); out.chipTone = 1; break;
        case DockTrafficPhase::EntryCleared: out.chip = tr("cockpit.dock_widget.chip_entry_cleared"); out.chipTone = 2; break;
        case DockTrafficPhase::StationControl: out.chip = tr("cockpit.dock_widget.chip_station"); out.chipTone = 2; break;
        case DockTrafficPhase::Docked: out.chip = tr("cockpit.dock_widget.chip_dock", {in.padId}); out.chipTone = 2; break;
        case DockTrafficPhase::DepartureQueued: out.chip = tr("cockpit.dock_widget.chip_departure_queue"); out.chipTone = 1; break;
        case DockTrafficPhase::DepartureCleared:
        case DockTrafficPhase::Departed: out.chip = tr("cockpit.dock_widget.chip_station_guiding"); out.chipTone = 2; break;
        default: break;
    }
    if (departing)
    {
        if (in.releaseKnown)
            out.fields.push_back({tr("cockpit.dock_widget.field_to_release"), dockingWidgetDistance(text, in.releaseDistanceMeters)});
        out.fields.push_back({tr("cockpit.dock_widget.field_speed"), speed()});
        if (in.beyondGate)
            out.fields.push_back({tr("cockpit.dock_widget.field_takeover"), tr("cockpit.dock_widget.value_takeover")});
    }
    if (!in.failure.empty())
    {
        out.chip = tr("cockpit.dock_widget.chip_error");
        out.chipTone = 3;
    }
    if (in.takeoverSecondsLeft >= 0.0)
    {
        out.chip = tr("cockpit.dock_widget.chip_takeover",
                      {tr("cockpit.unit.time_s", {TerminalLocalizedText::number(std::ceil(in.takeoverSecondsLeft), 0)})});
        out.chipTone = 3;
    }
    if (showDistance && in.distanceKnown)
    {
        out.fields.push_back({tr("cockpit.dock_widget.field_to_entry"), dockingWidgetDistance(text, in.distanceToEntryMeters)});
        out.fields.push_back({tr("cockpit.dock_widget.field_speed"), speed()});
    }
    if (waiting)
    {
        if (ph == DockTrafficPhase::Queued && in.queuePosition > 0)
            out.fields.push_back({tr("cockpit.dock_widget.field_queue"), std::to_string(in.queuePosition) + " / " + std::to_string(in.queueLength)});
        else if (ph == DockTrafficPhase::DepartureQueued)
            out.fields.push_back({tr("cockpit.dock_widget.field_queue"), std::to_string(in.departureQueuePosition)});
        else
            out.fields.push_back({tr("cockpit.dock_widget.field_ahead"), std::to_string(in.movementsAhead)});
        out.fields.push_back({tr("cockpit.dock_widget.field_wait"), dockingWidgetWait(text, in.estimatedWaitSeconds)});
    }
    if (dispatcher && !departing && (!in.holdPointId.empty() || !in.padId.empty()) && ph != DockTrafficPhase::Docked)
    {
        std::string place = in.holdPointId.empty() || in.holdPointId == "DIRECT" ? std::string() : in.holdPointId;
        if (!in.padId.empty())
            place += (place.empty() ? "" : " > ") + in.padId;
        if (!place.empty())
            out.fields.push_back({tr("cockpit.dock_widget.field_place"), place});
    }
    if (ph == DockTrafficPhase::Docked)
        out.fields.push_back({tr("cockpit.dock_widget.field_departure"), "Ctrl+L"});
    return out;
}

} // namespace game::presentation
