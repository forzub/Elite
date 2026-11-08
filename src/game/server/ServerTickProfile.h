#pragma once

#include <chrono>

namespace game::server
{
// DEV: where a server tick's wall time goes (EliteServer --load-test prints it). Accumulated
// seconds since the last reset; the stages are timed in GameServer::update / ServerRunner.
struct ServerTickProfile
{
    double simulation = 0.0;          // all system passes
    double sensors = 0.0;             // navigation sensor devices
    double snapshotBuild = 0.0;       // the shared replication snapshot + the canonical merge
    double sessionPlan = 0.0;         // per-session interest plans
    double sessionSelect = 0.0;       // per-session publication selection
    double sessionCopy = 0.0;         // per-session sparse snapshot copies
    double sessionPublish = 0.0;      // transport publish (wire encoding)
    double inbound = 0.0;             // client messages / map / time sync intake
    double mapRequests = 0.0;         // galaxy / system map responses
    long long ticks = 0;
    // GameSimulation::update stages, summed over the system passes: 0 ship maintenance, 1 hubs,
    // 2 hub frames + free flight, 3 static objects, 4 AI / controls, 5 physics, 6 signals / radar
    double simStage[7] {};
    // snapshot build: 0 simulation DTOs, 1 session data, 2 canonical merge
    double snapshotParts[3] {};
    double economy = 0.0;             // DEV economy ticks (ELITE_DEV_ECONOMY_TICK_S)
    long long economyTicks = 0;
    long long economyQuotes = 0;
};

inline ServerTickProfile& serverTickProfile()
{
    static ServerTickProfile profile;
    return profile;
}

struct ScopedTickTimer
{
    explicit ScopedTickTimer(double& sink) : m_sink(sink), m_start(std::chrono::steady_clock::now()) {}
    ~ScopedTickTimer() { m_sink += std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count(); }
    double& m_sink;
    std::chrono::steady_clock::time_point m_start;
};
} // namespace game::server
