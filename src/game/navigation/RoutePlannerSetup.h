#pragma once

// The orchestration's side of the route planner's options (RoutePlannerOptions, audit TST-2): the environment
// (ELITE_ROUTE_TIME_OBJECTIVE=0 the old weighted mix, ELITE_ROUTE_DRAFT=0 no draft net, ELITE_ROUTE_DRAFT_DUMP=<file>),
// the model file, the log - read HERE, once, by the game client and the server at start; the pure planner never reads
// them itself.

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include "src/game/navigation/planner/RouteDraftNet.h"
#include "src/game/navigation/planner/RoutePlannerApi.h"

namespace game::navigation
{
inline void configureRoutePlannerFromEnvironment()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    planner::RoutePlannerOptions o;
    const char* time = std::getenv("ELITE_ROUTE_TIME_OBJECTIVE");
    o.timeObjective = !(time && time[0] == '0');
    const char* draft = std::getenv("ELITE_ROUTE_DRAFT");
    if (!(draft && draft[0] == '0'))
    {
        static planner::RouteDraftNet net;
        const bool ok = net.load("assets/data/navigation/route_draft_net.json");
        std::cerr << "[RouteDraft] net " << (ok ? std::string("loaded") : "REFUSED / absent: " + net.rejectReason()) << std::endl;
        if (ok)
            o.draftNet = &net;
    }
    if (const char* dump = std::getenv("ELITE_ROUTE_DRAFT_DUMP"); dump && *dump)
        o.draftDumpPath = dump;
    o.log = [](const std::string& line) { std::cerr << line << std::endl; };
    o.budgetClockSeconds = [] {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    };
    planner::RoutePlanner::setOptions(std::move(o));
}
} // namespace game::navigation
