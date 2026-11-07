#pragma once

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "src/game/docking/traffic/DockTrafficControllerApi.h"

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

[[nodiscard]] inline std::string dockingWidgetDistance(double meters)
{
    char b[32];
    if (meters >= 1000.0)
        std::snprintf(b, sizeof b, "%.1f км", meters / 1000.0);
    else
        std::snprintf(b, sizeof b, "%.0f м", meters);
    return b;
}

[[nodiscard]] inline std::string dockingWidgetWait(double seconds)
{
    char b[32];
    if (seconds < 60.0)
        std::snprintf(b, sizeof b, "~%.0f с", std::max(0.0, seconds));
    else
        std::snprintf(b, sizeof b, "~%.0f мин", std::ceil(seconds / 60.0));
    return b;
}

[[nodiscard]] inline DockingWidgetText dockingWidgetText(const DockingWidgetInput& in)
{
    using game::docking::traffic::DockTrafficPhase;
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
    out.title = departing || (in.justReleased && !dispatcher) ? "ВЫЛЕТ" : "СТЫКОВКА";
    if (in.justReleased && !dispatcher && !in.requestPending)
    {
        out.chip = "УПРАВЛЕНИЕ ВАШЕ";
        out.chipTone = 2;
        out.consoleLine = "ВЫЛЕТ: управление у пилота";
        out.lines.push_back(out.consoleLine);
        char v[32];
        std::snprintf(v, sizeof v, "%.0f м/с", in.speedMps);
        out.fields.push_back({"СКОРОСТЬ", v});
        return out;
    }

    const auto clientStatus = [&]() -> std::string {
        switch (in.clientPhase)
        {
            case DockingWidgetClientPhase::Stabilizing: return "подготовка: гасим скорость";
            case DockingWidgetClientPhase::Planning:
            case DockingWidgetClientPhase::RouteReady: return "идёт расчёт маршрута";
            case DockingWidgetClientPhase::Executing: return "движемся к доку";
            case DockingWidgetClientPhase::SecuringCourse: return "выравниваем курс";
            default: return "запрос диспетчеру";
        }
    };
    std::string status;
    bool waiting = false;
    bool showDistance = true;
    switch (dispatcher ? in.phase : DockTrafficPhase::None)
    {
        case DockTrafficPhase::None: status = clientStatus(); break;
        case DockTrafficPhase::Queued:
            status = "в очереди: ждём точку ожидания";
            waiting = true;
            break;
        case DockTrafficPhase::HoldAssigned:
            status = "док занят - летим в точку ожидания " + in.holdPointId;
            waiting = true;
            break;
        case DockTrafficPhase::AtHold:
            status = "ожидание в точке " + in.holdPointId;
            waiting = true;
            break;
        case DockTrafficPhase::EntryCleared:
            status = "вход разрешён: " + clientStatus();
            break;
        case DockTrafficPhase::StationControl:
            status = "станция ведёт на " + in.padId + ", руль у станции";
            showDistance = false;
            break;
        case DockTrafficPhase::Docked:
            status = "пристыкован, площадка " + in.padId + "  ·  Ctrl+L - вылет";
            showDistance = false;
            break;
        case DockTrafficPhase::DepartureQueued:
            status = "вылет в очереди: " + std::to_string(in.departureQueuePosition) + "-й";
            waiting = true;
            showDistance = false;
            break;
        case DockTrafficPhase::DepartureCleared:
        case DockTrafficPhase::Departed:
            // no changing numbers here: the console prints every new phrase
            status = in.beyondGate ? "за воротами; руль 3 с - взять управление" : "станция выводит корабль из дока";
            showDistance = false;
            break;
        default: break;
    }
    if (!in.failure.empty())
        status = "ошибка: " + in.failure;
    out.lines.push_back(status);

    if (showDistance && in.distanceKnown)
    {
        char b[96];
        std::snprintf(b, sizeof b, "до входа %s  ·  скорость %.0f м/с", dockingWidgetDistance(in.distanceToEntryMeters).c_str(), in.speedMps);
        out.lines.push_back(b);
    }
    if (waiting)
    {
        std::string q;
        if (in.phase == DockTrafficPhase::Queued && in.queuePosition > 0)
            q = "очередь в док: " + std::to_string(in.queuePosition) + "-й из " + std::to_string(in.queueLength) + "  ·  ";
        q += "впереди манёвров: " + std::to_string(in.movementsAhead) + "  ·  ожидание " + dockingWidgetWait(in.estimatedWaitSeconds);
        out.lines.push_back(q);
    }
    else if (dispatcher && !in.padId.empty() &&
             (in.phase == DockTrafficPhase::EntryCleared))
        out.lines.push_back("площадка " + in.padId);

    out.consoleLine = (departing ? "ВЫЛЕТ: " : "СТЫКОВКА: ") + status;
    // ---- the panel ----
    const DockTrafficPhase ph = dispatcher ? in.phase : DockTrafficPhase::None;
    switch (ph)
    {
        case DockTrafficPhase::None:
            out.chip = in.clientPhase == DockingWidgetClientPhase::Planning || in.clientPhase == DockingWidgetClientPhase::RouteReady
                ? "РАСЧЁТ" : in.clientPhase == DockingWidgetClientPhase::Executing ? "В ПУТИ" : "ЗАПРОС";
            break;
        case DockTrafficPhase::Queued: out.chip = "ОЧЕРЕДЬ"; out.chipTone = 1; break;
        case DockTrafficPhase::HoldAssigned: out.chip = "К ТОЧКЕ " + in.holdPointId; out.chipTone = 1; break;
        case DockTrafficPhase::AtHold: out.chip = "ОЖИДАНИЕ " + in.holdPointId; out.chipTone = 1; break;
        case DockTrafficPhase::EntryCleared: out.chip = "ВХОД РАЗРЕШЁН"; out.chipTone = 2; break;
        case DockTrafficPhase::StationControl: out.chip = "СТАНЦИЯ"; out.chipTone = 2; break;
        case DockTrafficPhase::Docked: out.chip = "ДОК " + in.padId; out.chipTone = 2; break;
        case DockTrafficPhase::DepartureQueued: out.chip = "ВЫЛЕТ: ОЧЕРЕДЬ"; out.chipTone = 1; break;
        case DockTrafficPhase::DepartureCleared:
        case DockTrafficPhase::Departed: out.chip = "СТАНЦИЯ ВЕДЁТ"; out.chipTone = 2; break;
        default: break;
    }
    if (departing)
    {
        char v[32];
        std::snprintf(v, sizeof v, "%.0f м/с", in.speedMps);
        if (in.releaseKnown)
            out.fields.push_back({"ДО ВЫПУСКА", dockingWidgetDistance(in.releaseDistanceMeters)});
        out.fields.push_back({"СКОРОСТЬ", v});
        if (in.beyondGate)
            out.fields.push_back({"ПЕРЕХВАТ", "руль 3 с"});
    }
    if (!in.failure.empty())
    {
        out.chip = "ОШИБКА";
        out.chipTone = 3;
    }
    if (in.takeoverSecondsLeft >= 0.0)
    {
        char c[48];
        std::snprintf(c, sizeof c, "ПЕРЕХВАТ %.0f с", std::ceil(in.takeoverSecondsLeft));
        out.chip = c;
        out.chipTone = 3;
    }
    if (showDistance && in.distanceKnown)
    {
        char v[32];
        std::snprintf(v, sizeof v, "%.0f м/с", in.speedMps);
        out.fields.push_back({"ДО ВХОДА", dockingWidgetDistance(in.distanceToEntryMeters)});
        out.fields.push_back({"СКОРОСТЬ", v});
    }
    if (waiting)
    {
        if (ph == DockTrafficPhase::Queued && in.queuePosition > 0)
            out.fields.push_back({"ОЧЕРЕДЬ", std::to_string(in.queuePosition) + " / " + std::to_string(in.queueLength)});
        else if (ph == DockTrafficPhase::DepartureQueued)
            out.fields.push_back({"ОЧЕРЕДЬ", std::to_string(in.departureQueuePosition)});
        else
            out.fields.push_back({"ВПЕРЕДИ", std::to_string(in.movementsAhead)});
        out.fields.push_back({"ЖДАТЬ", dockingWidgetWait(in.estimatedWaitSeconds)});
    }
    if (dispatcher && !departing && (!in.holdPointId.empty() || !in.padId.empty()) && ph != DockTrafficPhase::Docked)
    {
        std::string place = in.holdPointId.empty() || in.holdPointId == "DIRECT" ? std::string() : in.holdPointId;
        if (!in.padId.empty())
            place += (place.empty() ? "" : " > ") + in.padId;
        if (!place.empty())
            out.fields.push_back({"МЕСТО", place});
    }
    if (ph == DockTrafficPhase::Docked)
        out.fields.push_back({"ВЫЛЕТ", "Ctrl+L"});
    return out;
}

} // namespace game::presentation
