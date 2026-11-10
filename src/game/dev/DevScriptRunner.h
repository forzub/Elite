#pragma once

// DEV-only scripted acceptance driver of the client (moved out of
// SpaceState::update, audit 2026-11-06 rec. 8 / 10).
//
// It replays, at fixed times after the client started, what a human does with
// clicks and keys: ELITE_DEV_SOL_MAP_SCRIPT="t:verb;t:verb;..." (map, panel,
// route, autopilot, flight and report verbs), ELITE_DEV_GALAXY_MAP_AT,
// ELITE_DEV_SYSTEM_MAP_AT, ELITE_DEV_SERVICE_SCRIPT, ELITE_DEV_DOCK_REQUEST_AT,
// ELITE_DEV_FREE_FLIGHT_AT (+ ELITE_DEV_FF_TRACE_SECONDS),
// ELITE_DEV_FREE_FLIGHT_THRUST_AT (+ _CMD) and ELITE_DEV_DRONE_VIEW_AT.
// Every variable is read once, at the first update. The world-specific DEV
// aliases (Sol body / hub ids) live in its data table, not in SpaceState.
//
// Interface to SpaceState: one call per client frame, SpaceState::update ->
// DevScriptRunner::instance().update(*this). The runner is a friend of
// SpaceState and drives it only through the same members a click / key path
// uses. Its state has process lifetime (one-shot latches), exactly like the
// function-local statics it replaces.

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

class SpaceState;

namespace game::dev
{

class DevScriptRunner
{
public:
    static DevScriptRunner& instance();

    // Runs every DEV step that is due. Called once per client frame before
    // the session advances.
    void update(SpaceState& s);

private:
    struct Config
    {
        double galaxyMapAtSeconds = -1.0;
        double systemMapAtSeconds = -1.0;
        std::vector<std::pair<double, std::string>> solMapScript;
        double serviceScriptAtSeconds = -1.0;
        double dockRequestAtSeconds = -1.0;
        double freeFlightAtSeconds = -1.0;
        double freeFlightTraceSeconds = 3.0;
        double freeFlightThrustAtSeconds = -1.0;
        int freeFlightThrustCommand = 1;
        double droneViewAtSeconds = -1.0;
        // ELITE_DEV_DESTROY_MODULES_AT=<s>:<module id>,<module id>... (step 3 S4 live breach test)
        double destroyModulesAtSeconds = -1.0;
        std::vector<std::string> destroyModules;
    };

    DevScriptRunner();
    static Config readConfig();

    void openMapsWhenDue(SpaceState& s);
    void runSolMapScript(SpaceState& s);
    void runServiceScript(SpaceState& s);
    void requestDockingWhenDue(SpaceState& s);
    void toggleFreeFlightWhenDue(SpaceState& s);
    void traceFreeFlight(SpaceState& s);
    void latchFreeFlightThrustWhenDue(SpaceState& s);
    void openDroneViewWhenDue(SpaceState& s);
    void destroyModulesWhenDue(SpaceState& s);

    // ELITE_DEV_SOL_MAP_SCRIPT verbs, in the original dispatch order; each
    // returns false when the verb is not its own.
    static bool runMapVerb(SpaceState& s, const std::string& cmd);
    static bool runRouteVerb(SpaceState& s, const std::string& cmd);
    static bool runObjectListVerb(SpaceState& s, const std::string& cmd);
    static bool runShipVerb(SpaceState& s, const std::string& cmd);
    static bool runReportVerb(SpaceState& s, const std::string& cmd);

    Config m_config;
    bool m_galaxyOpened = false;
    bool m_systemOpened = false;
    std::size_t m_nextSolMapStep = 0;
    int m_serviceStep = 0;
    bool m_dockRequestSent = false;
    bool m_freeFlightSent = false;
    double m_freeFlightTraceStart = -1.0;
    bool m_freeThrustSent = false;
    bool m_droneSet = false;
    bool m_modulesDestroyed = false;
};

} // namespace game::dev
