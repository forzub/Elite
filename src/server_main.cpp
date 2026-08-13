#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

#include "src/core/ConsoleOutput.h"
#include "src/game/server/HeadlessServerEndpoints.h"
#include "src/game/server/ServerRuntime.h"
#include "src/world/WorldParams.h"

namespace
{
std::atomic<bool> g_running {true};

void handleTerminationSignal(int)
{
    g_running.store(false);
}

void printUsage()
{
    std::cout
        << "EliteServer headless authoritative runtime\n"
        << "Usage:\n"
        << "  EliteServer.exe               Run until Ctrl+C/SIGTERM\n"
        << "  EliteServer.exe --self-test   Boot and advance the real server, then exit\n"
        << "  EliteServer.exe --help        Show this help\n";
}

int runHeadlessSelfTest()
{
    core::disableRuntimeStdoutNoise();

    game::server::HeadlessServerTransport transport;
    game::server::HeadlessDebugChannel debugChannel;
    WorldParams worldParams;

    std::cerr << "[SELFTEST] headless-server stage=construct-runtime\n";
    game::server::ServerRuntime runtime(
        worldParams,
        transport,
        debugChannel
    );

    if (!transport.hasSessionWelcome() ||
        !transport.hasBootstrapSnapshot() ||
        !debugChannel.hasBootstrapState())
    {
        std::cerr
            << "[FAIL] headless-server bootstrap protocol/debug publication missing\n";
        return 2;
    }

    const auto welcome = transport.sessionWelcome();
    if (welcome.controlledEntityId.value == 0 ||
        welcome.starAtlasCatalog.schemaVersion == 0 ||
        welcome.starAtlasCatalog.contentFingerprint == 0)
    {
        std::cerr
            << "[FAIL] headless-server invalid authoritative session bootstrap\n";
        return 3;
    }

    const auto initialTick =
        transport.latestSnapshot().metadata.serverTick;
    const auto initialPublicationCount =
        transport.snapshotPublicationCount();

    const double step = runtime.fixedStepSeconds();
    if (!std::isfinite(step) || step <= 0.0)
    {
        std::cerr << "[FAIL] headless-server invalid fixed step\n";
        return 4;
    }

    // Cross at least two normal replication publications (current cadence is
    // every three authoritative ticks) instead of merely constructing headers.
    for (int i = 0; i < 8; ++i)
        runtime.advance(step);

    const auto finalTick =
        transport.latestSnapshot().metadata.serverTick;
    const auto finalPublicationCount =
        transport.snapshotPublicationCount();

    if (finalTick <= initialTick ||
        finalPublicationCount <= initialPublicationCount)
    {
        std::cerr
            << "[FAIL] headless-server real authoritative runtime did not advance"
            << " initial_tick=" << initialTick
            << " final_tick=" << finalTick
            << " initial_publications=" << initialPublicationCount
            << " final_publications=" << finalPublicationCount
            << "\n";
        return 5;
    }

    std::cerr
        << "[PASS] headless-server executable boot + authoritative fixed-step smoke"
        << " final_tick=" << finalTick
        << "\n";
    return 0;
}

int runHeadlessServer()
{
    game::server::HeadlessServerTransport transport;
    game::server::HeadlessDebugChannel debugChannel;
    WorldParams worldParams;

    game::server::ServerRuntime runtime(
        worldParams,
        transport,
        debugChannel
    );

    std::signal(SIGINT, handleTerminationSignal);
#ifdef SIGTERM
    std::signal(SIGTERM, handleTerminationSignal);
#endif

    std::cout
        << "[EliteServer] authoritative headless runtime started"
        << " fixed_step_s=" << runtime.fixedStepSeconds()
        << "\n";
    std::cout
        << "[EliteServer] no remote socket transport exists yet; "
        << "running world simulation with empty inbound queues\n";

    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();

    while (g_running.load())
    {
        const auto now = Clock::now();
        const double elapsed =
            std::chrono::duration<double>(now - previous).count();
        previous = now;

        runtime.advance(elapsed);

        // ServerRunner owns fixed-step debt/catch-up.  This sleep only avoids
        // busy-spinning the process while there is no socket event loop yet.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "[EliteServer] stopped\n";
    return 0;
}
}

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        const std::string arg = argv[1];
        if (arg == "--self-test")
            return runHeadlessSelfTest();
        if (arg == "--help" || arg == "-h")
        {
            printUsage();
            return 0;
        }

        std::cerr << "[EliteServer] unknown option: " << arg << "\n";
        printUsage();
        return 1;
    }

    return runHeadlessServer();
}
