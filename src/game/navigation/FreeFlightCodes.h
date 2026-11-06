#pragma once

// Named codes of the free-flight state replicated to the client (architecture audit 2026-11-06,
// item 3: modes on bare numbers). The numeric values are the wire values (DynamicMotionState
// freeFlightCaptureRefusal / freeFlightAutopilotPhase) - never renumber, only append.
#include <cstdint>

namespace game::navigation
{

// why the last flight request was refused / an engine dropped (motion.freeFlightCaptureRefusal)
enum class FlightRefusal : std::uint8_t
{
    None = 0,
    NoLocalFrame = 1,          // stabilization: no hub within the capture envelope
    CaptureTooFast = 2,        // stabilization: too fast against the hub
    JDroppedNearObject = 3,    // the J field collapsed: an object's keep-out entered
    BelowJFloorMainEngines = 4,// below the J floor: the main engines fly
    LandingThrustTooLow = 5,   // landing refused: the main engines cannot land and take off again
    LandingAtmosphere = 6,     // landing refused: an atmosphere (only shuttles)
    JRefusedNearObject = 7,    // J refused: within a hub / station (20 km) or ship (5 km) keep-out
    JRefusedBelowFloor = 8,    // J refused: below a body's J floor
    JChargeEmpty = 9           // J refused / collapsed: the jump-drive charge is empty (or no jump core)
};

// what the autopilot panel shows (motion.freeFlightAutopilotPhase)
enum class AutopilotDisplayPhase : std::uint8_t
{
    None = 0,
    Departure = 1,             // уход от тела
    Transfer = 2,              // перелёт
    Approach = 3,              // подход
    PadDescent = 4,            // снижение на площадку
    OnSurface = 5,             // на поверхности
    ToPad = 6,                 // к площадке
    SiteChoice = 7,            // выбор площадки
    OrbitSiteSearch = 8,       // орбита: нет ровной площадки, поиск
    HubExit = 9,               // выход из зоны хаба (J мягко) - route legs
    GroundStabilization = 10,  // стабилизация над грунтом
    Deorbit = 11,              // сход с орбиты (нормальный полёт, маршевые)
    OrbitHold = 12             // на орбите (стабилизация), ждём схода
};

constexpr std::uint8_t code(FlightRefusal r) { return static_cast<std::uint8_t>(r); }
constexpr std::uint8_t code(AutopilotDisplayPhase p) { return static_cast<std::uint8_t>(p); }

} // namespace game::navigation
