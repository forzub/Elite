#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "core/Application.h"
#include "game/server/GameServer.h"
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
            << "accelerated timeline entry\n";
        return 2;
    }

    server->setDebugUniverseTimeSimulation(false, TestScale);
    server->update(StepSeconds);

    const auto restored = server->protocolMetadata();
    const auto& restoredSnapshot = server->snapshot();

    const bool exited =
        !server->debugUniverseTimeSimulation() &&
        std::abs(server->debugUniverseTimeScale() - 1.0) < 1.0e-9 &&
        restored.universeTimelineRevision > accelerated.universeTimelineRevision &&
        restored.universeTimeSeconds < accelerated.universeTimeSeconds &&
        !restoredSnapshot.session.universeTimeSimulation &&
        std::abs(restoredSnapshot.session.universeTimeScale - 1.0) < 1.0e-9;

    if (!exited)
    {
        std::cerr
            << "[FAIL] fast-universe smoke: real server did not return "
            << "to normal timeline\n";
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

    try
    {

        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];

            if (isFastUniverseSmokeTestToken(arg))
                return runFastUniverseSmokeTest();

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
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}