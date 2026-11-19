#pragma once

// Refuelling at a station (author 2026-11-19: «найди заправки, заправь корабль на них»; docs/FUEL_STATIONS.md).
//
// What is filled: the jump charge (t) - the ship's only runtime consumable (std.tank.jumpcharge tanks, JDrive.h). A job
// is a small state machine the SERVER runs per ship (GameSimulationRefuel.cpp): the coupling (hookup: the hose or the
// tender's boom, leak test, purge), the pumping at the station's rate until the tank is full or the station's stock
// runs out, the disconnect. The rates come from the station data (src/assets/data/fuel/fuel_stations.json): a hub's
// refuel dock pumps 1.2 t/min (~630 L/min of a 1.9 t/m3 hazard-D charge - half an airliner's single-point refuelling),
// a transit depot 1.8, a small dock 0.6, an illegal hose from a tanker's hold 0.4. A full Cobrett (3 t) at a hub:
// 30 s + 2.5 min + 15 s.
//
// Pure: no simulation types - the unit test (tests/fuel/RefuelRulesTests.cpp) drives it directly.
#include <algorithm>
#include <cstdint>

namespace game::fuel
{
enum class RefuelPhase : std::uint8_t
{
    None = 0,
    Hookup = 1,       // coupling the hose / the tender's boom
    Pumping = 2,
    Disconnect = 3,
    Done = 4,         // ended normally (full, or the station ran dry - deliveredT tells how much)
    Refused = 5,      // never started: RefuelRefusal says why
    Cancelled = 6     // stopped by the pilot, a departure or a move (what was pumped stays in the tank)
};

enum class RefuelRefusal : std::uint8_t
{
    None = 0,
    NotAtStation = 1,     // not docked on a pad / not held in a hub's frame
    NoFuelHere = 2,       // the station does not sell jump charge (or is not a world object)
    TankFull = 3,
    NoJumpDrive = 4,      // the ship has no charge tanks
    StationDry = 5,       // the station's stock is empty
    PaymentRefused = 6,   // the account could not pay (economy interface)
    Busy = 7,             // a jump is being charged / under way
};

[[nodiscard]] inline const char* refuelPhaseName(RefuelPhase p)
{
    switch (p)
    {
        case RefuelPhase::None: return "none";
        case RefuelPhase::Hookup: return "hookup";
        case RefuelPhase::Pumping: return "pumping";
        case RefuelPhase::Disconnect: return "disconnect";
        case RefuelPhase::Done: return "done";
        case RefuelPhase::Refused: return "refused";
        case RefuelPhase::Cancelled: return "cancelled";
    }
    return "?";
}

[[nodiscard]] inline const char* refuelRefusalName(RefuelRefusal r)
{
    switch (r)
    {
        case RefuelRefusal::None: return "none";
        case RefuelRefusal::NotAtStation: return "not docked at a station";
        case RefuelRefusal::NoFuelHere: return "no jump charge sold here";
        case RefuelRefusal::TankFull: return "the tank is full";
        case RefuelRefusal::NoJumpDrive: return "no jump-charge tanks";
        case RefuelRefusal::StationDry: return "the station is out of jump charge";
        case RefuelRefusal::PaymentRefused: return "payment refused";
        case RefuelRefusal::Busy: return "a jump is under way";
    }
    return "?";
}

[[nodiscard]] inline bool refuelActive(RefuelPhase p)
{
    return p == RefuelPhase::Hookup || p == RefuelPhase::Pumping || p == RefuelPhase::Disconnect;
}

struct PumpSpec
{
    double rateTPerMin = 1.2;
    double hookupS = 30.0;
    double disconnectS = 15.0;
};

// a tender (bunker boat) serving a ship held in a hub's frame - like bunkering at anchor - first has to come alongside:
// the coupling takes this many times longer than at a dock's fixed hose
constexpr double TenderHookupFactor = 2.0;
// the ship held in a hub's frame must stay nearly still against it for a tender's boom (m/s)
constexpr double TenderMaxRelativeSpeedMps = 5.0;
constexpr double FullEpsilonT = 1.0e-6;

struct RefuelJob
{
    RefuelPhase phase = RefuelPhase::Hookup;
    double phaseSeconds = 0.0;
    double deliveredT = 0.0;
};

// One server tick of a job. `attached` false (a tender's boom: the ship drifts too fast) pauses the coupling / the
// pumping. Fills `chargeT` up to `capacityT` from `stationStockT` (both changed); returns the tonnes moved this tick.
inline double refuelStep(RefuelJob& job, const PumpSpec& pump, double dt, bool attached, double& chargeT, double capacityT,
                         double& stationStockT)
{
    if (dt <= 0.0 || !refuelActive(job.phase))
        return 0.0;
    if (!attached && job.phase != RefuelPhase::Disconnect)
        return 0.0;
    switch (job.phase)
    {
        case RefuelPhase::Hookup:
            job.phaseSeconds += dt;
            if (job.phaseSeconds >= pump.hookupS)
            {
                job.phase = RefuelPhase::Pumping;
                job.phaseSeconds = 0.0;
            }
            return 0.0;
        case RefuelPhase::Pumping:
        {
            job.phaseSeconds += dt;
            const double room = std::max(0.0, capacityT - chargeT);
            const double add = std::min({std::max(0.0, pump.rateTPerMin) / 60.0 * dt, room, std::max(0.0, stationStockT)});
            chargeT += add;
            stationStockT -= add;
            job.deliveredT += add;
            if (capacityT - chargeT <= FullEpsilonT || stationStockT <= FullEpsilonT || pump.rateTPerMin <= 0.0)
            {
                if (capacityT - chargeT <= FullEpsilonT)
                    chargeT = capacityT;
                job.phase = RefuelPhase::Disconnect;
                job.phaseSeconds = 0.0;
            }
            return add;
        }
        case RefuelPhase::Disconnect:
            job.phaseSeconds += dt;
            if (job.phaseSeconds >= pump.disconnectS)
                job.phase = RefuelPhase::Done;
            return 0.0;
        default:
            return 0.0;
    }
}

// the whole job's duration from an empty-ish tank (the HUD's estimate): coupling + pumping + disconnect
[[nodiscard]] inline double refuelEtaSeconds(const PumpSpec& pump, double needT)
{
    return pump.hookupS + (pump.rateTPerMin > 0.0 ? std::max(0.0, needT) / pump.rateTPerMin * 60.0 : 0.0) + pump.disconnectS;
}
} // namespace game::fuel
