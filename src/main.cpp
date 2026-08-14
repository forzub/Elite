#include <cmath>
#include <algorithm>
#include <utility>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include "core/Application.h"
#include "game/server/GameServer.h"
#include "game/diagnostics/ClientAcceptanceHarness.h"
#include "game/diagnostics/MultiplayerClientAcceptanceHarness.h"
#include "src/game/session/RemoteGameSession.h"
#include "src/game/network/NetworkEndpoint.h"
#include "src/game/client/GameClient.h"
#include "core/ConsoleOutput.h"
#include "world/celestial/visual/CelestialTextureBaker.h"
#include "render/bitmap/stb_image.h"

#include <clocale>



namespace
{

bool isBakeCommandToken(const std::string& arg)
{
    return arg == "--bake-celestial-textures" ||
           arg == "bake-celestial-textures";
}

bool isFastUniverseSmokeTestToken(const std::string& arg)
{
    return arg == "--self-test-fast-universe";
}

bool isClientAcceptanceSelfTestToken(const std::string& arg)
{
    return arg == "--self-test-client-acceptance";
}

bool isMultiplayerClientAcceptanceSelfTestToken(const std::string& arg)
{
    return arg == "--self-test-multiplayer-client";
}

int runRemoteClientProcessSelfTest(
    const game::network::NetworkEndpoint& endpoint)
{
    core::disableRuntimeStdoutNoise();

    game::session::RemoteGameSessionConfig config;
    config.host = endpoint.host;
    config.port = endpoint.port;
    game::session::RemoteGameSession session(std::move(config));
    session.beginSynchronization();

    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();

    // Client-first process acceptance has two distinct phases. Waiting for the
    // server process to finish booting is not session synchronization time:
    // authoritative startup may legitimately spend several seconds loading the
    // world and preparing CPU collision/structural geometry before listen().
    // Keep a bounded test-only wait, then start the synchronization deadline
    // only after TCP has actually become available.
    const auto serverWaitDeadline = previous + std::chrono::seconds(60);

    while (Clock::now() < serverWaitDeadline &&
           session.state() == game::session::GameSessionState::WaitingForServer)
    {
        const auto now = Clock::now();
        const double elapsed = std::clamp(
            std::chrono::duration<double>(now - previous).count(),
            0.0,
            0.05
        );
        previous = now;
        session.updateSynchronization(elapsed);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    if (session.state() == game::session::GameSessionState::WaitingForServer)
    {
        std::cerr
            << "[FAIL] remote-client process server did not become available"
            << " error=" << session.error() << "\n";
        return 2;
    }

    if (session.state() == game::session::GameSessionState::Failed)
    {
        std::cerr
            << "[FAIL] remote-client process failed while waiting for server: "
            << session.error() << "\n";
        return 2;
    }

    const auto syncDeadline = Clock::now() + std::chrono::seconds(10);

    while (Clock::now() < syncDeadline &&
           session.state() != game::session::GameSessionState::Ready &&
           session.state() != game::session::GameSessionState::Failed)
    {
        const auto now = Clock::now();
        const double elapsed = std::clamp(
            std::chrono::duration<double>(now - previous).count(),
            0.0,
            0.05
        );
        previous = now;
        session.updateSynchronization(elapsed);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    if (session.state() != game::session::GameSessionState::Ready)
    {
        std::cerr
            << "[FAIL] remote-client process synchronization failed after server connection: "
            << session.error() << "\n";
        return 2;
    }

    if (session.playerId().value == 0 ||
        !std::isfinite(session.fixedStepSeconds()) ||
        session.fixedStepSeconds() <= 0.0)
    {
        std::cerr
            << "[FAIL] remote-client process received invalid session authority/cadence\n";
        return 3;
    }

    ShipControlState control;
    control.forwardInput = 0.25f;
    session.client().submitInput(control);

    previous = Clock::now();
    const auto inputDeadline = previous + std::chrono::seconds(10);
    while (Clock::now() < inputDeadline &&
           session.client().lastAcknowledgedControlTick() == 0 &&
           session.state() != game::session::GameSessionState::Failed)
    {
        const auto now = Clock::now();
        const double elapsed = std::clamp(
            std::chrono::duration<double>(now - previous).count(),
            0.0,
            0.05
        );
        previous = now;

        (void)session.advance(elapsed);
        session.client().update(
            static_cast<float>(elapsed),
            static_cast<float>(session.fixedStepSeconds()),
            elapsed
        );
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    if (session.state() == game::session::GameSessionState::Failed ||
        session.client().lastAcknowledgedControlTick() == 0)
    {
        std::cerr
            << "[FAIL] remote-client process authoritative input acknowledgement missing"
            << " error=" << session.error() << "\n";
        return 4;
    }

    std::cerr
        << "[PASS] remote GameClient synchronized and exchanged authoritative input"
        << " player=" << session.playerId().value
        << " ack=" << session.client().lastAcknowledgedControlTick()
        << " fixed_step_s=" << session.fixedStepSeconds()
        << "\n";
    return 0;
}

int runFastUniverseSmokeTest()
{
    // This intentionally boots the real server and initial scene.
    // It catches integration failures that pure clock/session tests miss.
    core::disableRuntimeStdoutNoise();

    std::cerr << "[SELFTEST] fast-universe stage=construct-server\n";
    auto server = std::make_unique<GameServer>();
    std::cerr << "[SELFTEST] fast-universe stage=server-ready\n";

    const auto before = server->protocolMetadata();
    constexpr double TestScale = 200.0;
    constexpr double StepSeconds = 0.02;

    server->setDebugUniverseTimeSimulation(true, TestScale);
    server->update(StepSeconds);

    const auto accelerated = server->protocolMetadata();
    const auto& acceleratedSnapshot = server->snapshot();

    const bool entered =
        server->debugUniverseTimeSimulation() &&
        std::abs(server->debugUniverseTimeScale() - TestScale) < 1.0e-9 &&
        accelerated.universeTimelineRevision > before.universeTimelineRevision &&
        accelerated.universeTimeSeconds - before.universeTimeSeconds > 2.0 &&
        acceleratedSnapshot.session.universeTimeSimulation &&
        std::abs(acceleratedSnapshot.session.universeTimeScale - TestScale) < 1.0e-9;

    if (!entered)
    {
        std::cerr
            << "[FAIL] fast-universe smoke: real server/scene rejected "
            << "accelerated timeline entry"
            << " mode=" << server->debugUniverseTimeSimulation()
            << " scale=" << server->debugUniverseTimeScale()
            << " before_revision=" << before.universeTimelineRevision
            << " accelerated_revision=" << accelerated.universeTimelineRevision
            << " delta_universe_s="
            << (accelerated.universeTimeSeconds - before.universeTimeSeconds)
            << " snapshot_mode="
            << acceleratedSnapshot.session.universeTimeSimulation
            << " snapshot_scale="
            << acceleratedSnapshot.session.universeTimeScale
            << "\n";
        return 2;
    }

    server->setDebugUniverseTimeSimulation(false, TestScale);

    // Leaving the diagnostic branch re-anchors UniverseClock to wall time.
    // Do not compare that absolute timestamp with the accelerated timestamp:
    // server construction itself may take longer than the synthetic +4 s jump.
    // The invariant is that normal mode resumes at x1 and then advances by the
    // authoritative server step again.
    const auto restoredAtExit = server->protocolMetadata();

    server->update(StepSeconds);

    const auto restored = server->protocolMetadata();
    const auto& restoredSnapshot = server->snapshot();

    const double restoredNormalStep =
        restored.universeTimeSeconds - restoredAtExit.universeTimeSeconds;

    const bool exited =
        !server->debugUniverseTimeSimulation() &&
        std::abs(server->debugUniverseTimeScale() - 1.0) < 1.0e-9 &&
        restoredAtExit.universeTimelineRevision > accelerated.universeTimelineRevision &&
        restored.universeTimelineRevision == restoredAtExit.universeTimelineRevision &&
        restoredNormalStep >= 0.0 &&
        std::abs(restoredNormalStep - StepSeconds) < 0.01 &&
        !restoredSnapshot.session.universeTimeSimulation &&
        std::abs(restoredSnapshot.session.universeTimeScale - 1.0) < 1.0e-9;

    if (!exited)
    {
        std::cerr
            << "[FAIL] fast-universe smoke: real server did not return "
            << "to normal timeline"
            << " mode=" << server->debugUniverseTimeSimulation()
            << " scale=" << server->debugUniverseTimeScale()
            << " accelerated_revision=" << accelerated.universeTimelineRevision
            << " exit_revision=" << restoredAtExit.universeTimelineRevision
            << " post_tick_revision=" << restored.universeTimelineRevision
            << " normal_step_s=" << restoredNormalStep
            << " snapshot_mode="
            << restoredSnapshot.session.universeTimeSimulation
            << " snapshot_scale="
            << restoredSnapshot.session.universeTimeScale
            << "\n";
        return 3;
    }

    std::cerr << "[PASS] fast-universe real-scene smoke\n";
    return 0;
}

bool isOptionToken(const std::string& arg)
{
    return arg.rfind("--", 0) == 0;
}

std::string stripSingleDashFilter(const std::string& arg)
{
    if (arg.size() >= 2 &&
        arg[0] == '-' &&
        arg[1] != '-')
    {
        return arg.substr(1);
    }

    return arg;
}

void printCelestialBakeUsage()
{
    std::cout
        << "Celestial texture bake usage:\n"
        << "  EliteGame.exe --bake-celestial-textures\n"
        << "  EliteGame.exe --bake-celestial-textures --list\n"
        << "  EliteGame.exe --bake-celestial-textures --system sol\n"
        << "  EliteGame.exe --bake-celestial-textures --system sol --body earth\n"
        << "  EliteGame.exe --bake-celestial-textures --body sol/earth\n"
        << "  EliteGame.exe --bake-celestial-textures -sol\n"
        << "  EliteGame.exe --bake-celestial-textures -sol -earth\n"
        << "  EliteGame.exe --bake-celestial-textures --check-sources -sol\n"
        << "  EliteGame.exe --bake-celestial-textures --system sol --body earth --import-real\n";
}

bool parseCelestialBakeOptions(
    int argc,
    char** argv,
    int bakeArgIndex,
    world::celestial::visual::CelestialTextureBakeOptions& out
)
{
    std::vector<std::string> positionalFilters;

    for (int i = bakeArgIndex + 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            printCelestialBakeUsage();
            out.listOnly = true;
            return true;
        }

        if (arg == "--list")
        {
            out.listOnly = true;
            continue;
        }

        if (arg == "--system")
        {
            if (i + 1 >= argc)
            {
                std::cerr
                    << "[App] --system requires value\n";

                return false;
            }

            out.systemFilter = argv[++i];
            continue;
        }

        if (arg == "--body")
        {
            if (i + 1 >= argc)
            {
                std::cerr
                    << "[App] --body requires value\n";

                return false;
            }

            out.bodyFilter = argv[++i];
            continue;
        }

        if (arg == "--presets")
        {
            if (i + 1 >= argc)
            {
                std::cerr
                    << "[App] --presets requires path\n";

                return false;
            }

            out.presetsPath = argv[++i];
            continue;
        }


        if (arg == "--import-real")
        {
            out.allowImportedRealDataBake = true;
            continue;
        }


        if (arg == "--body-visuals")
        {
            if (i + 1 >= argc)
            {
                std::cerr
                    << "[App] --body-visuals requires path\n";

                return false;
            }

            out.bodyVisualsRoot = argv[++i];
            continue;
        }

        if (arg == "--output")
        {
            if (i + 1 >= argc)
            {
                std::cerr
                    << "[App] --output requires path\n";

                return false;
            }

            out.outputRoot = argv[++i];
            continue;
        }

        if (arg == "--check-sources")
        {
            out.checkSources = true;
            out.listOnly = true;
            continue;
        }

        // Shorthand filters:
        //
        //   -sol
        //   -earth
        //
        // Only single-dash unknown args are treated as positional filters.
        if (arg.size() >= 2 &&
            arg[0] == '-' &&
            arg[1] != '-')
        {
            positionalFilters.push_back(stripSingleDashFilter(arg));
            continue;
        }

        // Plain positional filters are also allowed:
        //
        //   --bake-celestial-textures sol earth
        //
        if (!isOptionToken(arg))
        {
            positionalFilters.push_back(arg);
            continue;
        }

        std::cerr
            << "[App] Unknown celestial bake option: "
            << arg
            << "\n";

        return false;
    }

    if (!positionalFilters.empty())
    {
        if (out.systemFilter.empty())
            out.systemFilter = positionalFilters[0];

        if (positionalFilters.size() >= 2 &&
            out.bodyFilter.empty())
        {
            out.bodyFilter = positionalFilters[1];
        }

        if (positionalFilters.size() > 2)
        {
            std::cerr
                << "[App] Too many positional celestial bake filters\n";

            return false;
        }
    }

    return true;
}

} // namespace








int main(int argc, char** argv)
{
    bool useRemoteServer = false;
    game::network::NetworkEndpoint remoteEndpoint;

    try
    {

        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];

            if (arg == "--connect" || arg == "--self-test-remote-client")
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "[App] " << arg << " requires HOST:PORT\n";
                    return -2;
                }

                std::string error;
                game::network::NetworkEndpoint endpoint;
                if (!game::network::parseNetworkEndpoint(
                        argv[++i], endpoint, &error))
                {
                    std::cerr << "[App] invalid remote endpoint: "
                              << error << "\n";
                    return -2;
                }

                if (arg == "--self-test-remote-client")
                    return runRemoteClientProcessSelfTest(endpoint);

                useRemoteServer = true;
                remoteEndpoint = std::move(endpoint);
                continue;
            }

            if (isFastUniverseSmokeTestToken(arg))
                return runFastUniverseSmokeTest();

            if (isClientAcceptanceSelfTestToken(arg))
            {
                core::disableRuntimeStdoutNoise();
                return game::diagnostics::runClientAcceptanceSelfTest();
            }

            if (isMultiplayerClientAcceptanceSelfTestToken(arg))
            {
                core::disableRuntimeStdoutNoise();
                return game::diagnostics::runMultiplayerClientAcceptanceSelfTest();
            }

            if (isBakeCommandToken(arg))
            {
                world::celestial::visual::CelestialTextureBakeOptions options;

                if (!parseCelestialBakeOptions(
                        argc,
                        argv,
                        i,
                        options))
                {
                    printCelestialBakeUsage();
                    return -2;
                }

                world::celestial::visual::CelestialTextureBaker baker;

                const bool ok =
                    baker.bakeLibrary(options);

                return ok ? 0 : -2;
            }
        }





        std::setlocale(LC_ALL, "");
        core::disableRuntimeStdoutNoise();
        Application app;
        if (useRemoteServer)
        {
            app.configureRemoteServer(
                remoteEndpoint.host,
                remoteEndpoint.port
            );
        }
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}