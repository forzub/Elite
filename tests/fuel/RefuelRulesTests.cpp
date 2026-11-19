// Refuel service rules and the fuel-station catalog (docs/FUEL_STATIONS.md). Pure: RefuelRules.h's job state machine
// and FuelStationCatalog against the real data file src/assets/data/fuel/fuel_stations.json.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "src/game/fuel/FuelStationCatalog.h"
#include "src/game/fuel/RefuelRules.h"

namespace
{
int g_failed = 0;
int g_checks = 0;

void check(bool ok, const std::string& what)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failed;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

bool near(double a, double b, double eps) { return std::abs(a - b) <= eps; }

using namespace game::fuel;

// run a job to its end at 10 Hz; returns the seconds it took
double runJob(RefuelJob& job, const PumpSpec& pump, double& charge, double capacity, double& stock, double maxS = 7200.0)
{
    double t = 0.0;
    while (refuelActive(job.phase) && t < maxS)
    {
        refuelStep(job, pump, 0.1, true, charge, capacity, stock);
        t += 0.1;
    }
    return t;
}

void testFullCobrettAtHub()
{
    // an empty Cobrett (3 t) at a hub's refuel dock: 30 s coupling + 3 t / 1.2 t/min = 150 s + 15 s disconnect
    PumpSpec pump {1.2, 30.0, 15.0};
    RefuelJob job;
    double charge = 0.0, stock = 1000.0;
    const double t = runJob(job, pump, charge, 3.0, stock);
    check(job.phase == RefuelPhase::Done, "hub job ends Done");
    check(near(charge, 3.0, 1e-9), "hub job fills the tank to capacity");
    check(near(job.deliveredT, 3.0, 1e-6), "delivered = the room in the tank");
    check(near(stock, 997.0, 1e-6), "the station's stock drops by the delivered charge");
    check(near(t, 195.0, 0.3), "hub job takes 30 + 150 + 15 s (got " + std::to_string(t) + ")");
    check(near(refuelEtaSeconds(pump, 3.0), 195.0, 1e-9), "ETA matches the job");
}

void testNotInstant()
{
    // plausible rate, not instant: after 60 s of a hub job only 30 s of pumping happened -> 0.6 t
    PumpSpec pump {1.2, 30.0, 15.0};
    RefuelJob job;
    double charge = 0.5, stock = 100.0;
    for (int i = 0; i < 600; ++i)
        refuelStep(job, pump, 0.1, true, charge, 3.0, stock);
    check(job.phase == RefuelPhase::Pumping, "still pumping after 60 s");
    check(near(charge, 0.5 + 0.6, 0.03), "0.6 t pumped in the first 30 s of pumping (got " + std::to_string(charge) + ")");
}

void testStationRunsDry()
{
    PumpSpec pump {1.2, 30.0, 15.0};
    RefuelJob job;
    double charge = 0.0, stock = 1.0;
    runJob(job, pump, charge, 3.0, stock);
    check(job.phase == RefuelPhase::Done, "dry station: the job still ends");
    check(near(charge, 1.0, 1e-6), "dry station: only its stock is delivered");
    check(stock <= FullEpsilonT, "dry station: stock exhausted");
}

void testDetachedPauses()
{
    // a tender's boom: the ship drifting too fast pauses the coupling and the pumping
    PumpSpec pump {1.2, 30.0, 15.0};
    RefuelJob job;
    double charge = 1.0, stock = 100.0;
    for (int i = 0; i < 1000; ++i)
        refuelStep(job, pump, 0.1, false, charge, 3.0, stock);
    check(job.phase == RefuelPhase::Hookup && job.phaseSeconds == 0.0, "detached: the coupling does not advance");
    for (int i = 0; i < 400; ++i)
        refuelStep(job, pump, 0.1, true, charge, 3.0, stock);   // 40 s: coupled + 10 s of pumping
    const double c0 = charge;
    for (int i = 0; i < 100; ++i)
        refuelStep(job, pump, 0.1, false, charge, 3.0, stock);
    check(charge == c0, "detached: the pumping pauses");
}

void testTenderAndIllegalSlower()
{
    PumpSpec hub {1.2, 30.0, 15.0};
    PumpSpec tender = hub;
    tender.hookupS *= TenderHookupFactor;
    PumpSpec illegal {0.4, 60.0, 20.0};
    check(refuelEtaSeconds(tender, 3.0) > refuelEtaSeconds(hub, 3.0), "a tender takes longer than a dock hose");
    check(near(refuelEtaSeconds(illegal, 3.0), 60.0 + 450.0 + 20.0, 1e-9), "illegal hose: 60 s + 7.5 min + 20 s");
}


std::string readFile(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

void testCatalog(const std::string& root)
{
    FuelStationCatalog c;
    check(!c.loadFromText("{not json"), "a broken file is refused");
    const std::string text = readFile(root + "/src/assets/data/fuel/fuel_stations.json");
    check(c.loadFromText(text), "the data file loads");
    check(c.stations().size() > 600, "every inhabited hub is a station (got " + std::to_string(c.stations().size()) + ")");
    // the runtime hubs a pilot docks at today sell fuel
    for (const char* id : {"earth_orbital_hub", "earth_hub.tikhiy", "site.003.wolf359_citadel.port01", "site.004.lalande_hellas.port01",
                           "site.041.vorota", "plane_demo"})
        check(c.sellsFuel(id), std::string("sells fuel: ") + id);
    check(!c.sellsFuel("plane_ruin"), "the ruined demo city does not sell fuel");
    check(c.find("nowhere") == nullptr, "an unknown hub is not a station");
    // the placement pass's refuelling stations
    for (const char* id : {"fuel.014.jump_point", "fuel.025.jump_point", "fuel.054.jump_point", "fuel.041.vorota_mouth", "fuel.037.kern_mouth",
                           "site.024.pereval"})
    {
        const auto* s = c.find(id);
        check(s && s->kind == "refuel_point" && !s->illegal && s->stockT > 0.0, std::string("refuel point: ") + id);
    }
    // illegal places: flagged, risky, dearer, slower, never offered to the route planner as an ordinary station
    int illegal = 0;
    for (const auto& s : c.stations())
        if (s.illegal)
        {
            ++illegal;
            check(s.legality == "illegal" && s.risk > 0.0 && s.priceFactor > 1.0, "illegal station flagged: " + s.id);
            check(s.pump.rateTPerMin < 1.0, "illegal hose is slow: " + s.id);
            check(!c.sellsFuel(s.id), "illegal not offered as an ordinary station: " + s.id);
        }
    check(illegal >= 17, "the 17 hidden sites (+ proposals) are illegal stations (got " + std::to_string(illegal) + ")");
    const auto* pirate = c.find("pir.012.1");
    check(pirate && pirate->illegal && !pirate->proposed, "a pirate settlement sells charge illegally");
    const auto* vulture = c.find("ill.004.sump");
    check(vulture && vulture->proposed, "a proposed sump vulture in Lalande");
}
} // namespace

int main(int argc, char** argv)
{
#ifndef ELITE_SOURCE_ROOT
#define ELITE_SOURCE_ROOT "."
#endif
    const std::string root = argc > 1 ? argv[1] : ELITE_SOURCE_ROOT;
    testFullCobrettAtHub();
    testNotInstant();
    testStationRunsDry();
    testDetachedPauses();
    testTenderAndIllegalSlower();
    testCatalog(root);
    std::printf("refuel_rules_tests: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
