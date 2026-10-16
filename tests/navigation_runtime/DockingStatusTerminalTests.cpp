#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "src/game/presentation/DockingStatusTerminal.h"

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
}

int main()
{
    try
    {
        using game::presentation::DockingStatusTerminal;
        using game::presentation::DockingTerminalSeverity;
        using game::presentation::DockingTerminalSnapshot;

        DockingStatusTerminal terminal;

        DockingTerminalSnapshot snapshot;
        snapshot.visible = true;
        snapshot.title = "DOCKING";
        snapshot.phase = "stabilization";
        snapshot.planner = "planner on 20%";
        snapshot.safety = "safe-course clear";

        auto out = terminal.update(snapshot, 0.0);
        require(
            out.typedCharacters == 0,
            "terminal advanced typewriter without elapsed time"
        );
        require(
            out.lines.size() <= 1,
            "terminal emitted completed history without elapsed time"
        );

        out = terminal.update(snapshot, 1.0);
        require(out.typedCharacters > 0, "terminal did not type characters");
        require(!out.lines.empty(), "terminal produced no visible line");

        snapshot.phase = "planning";
        snapshot.ship = "SHIP Assisted-I LAW ASSISTED";
        snapshot.physics = "ACT main=4 aft=2 fore=2 rcs=0";
        snapshot.activity = "CALC A1B2 C3D4 E5F6 0718";
        out = terminal.update(snapshot, 2.0);
        require(out.typedCharacters > 0, "changed status was not queued");

        snapshot.error = "error: handoff drift";
        for (int i = 3; i < 16; ++i)
            out = terminal.update(snapshot, static_cast<double>(i));

        bool sawError = false;
        for (const auto& line : out.lines)
        {
            if (line.text.find("handoff drift") != std::string::npos)
            {
                sawError = true;
                require(
                    line.severity == DockingTerminalSeverity::Error,
                    "terminal error severity was lost"
                );
            }
        }
        require(sawError, "terminal never surfaced queued error");

        snapshot.visible = false;
        out = terminal.update(snapshot, 16.0);
        require(out.lines.empty(), "hidden terminal emitted visible lines");

        // Hidden is not reset: physical-model switches and later docking
        // sessions continue the same scrolling diagnostic history.
        snapshot.visible = true;
        snapshot.ship = "SHIP Newtonian-I LAW NEWTONIAN";
        snapshot.physics = "ACT main=1 aft=1 fore=0 rcs=4";
        for (int i = 17; i < 24; ++i)
            out = terminal.update(snapshot, static_cast<double>(i));

        bool sawModelSwitch = false;
        for (const auto& line : out.lines)
        {
            if (line.text.find("Newtonian-I") != std::string::npos)
                sawModelSwitch = true;
        }
        require(
            sawModelSwitch,
            "terminal did not append physical-model switch after hidden interval"
        );

        std::cout << "DOCKING STATUS TERMINAL: PASS\n";
        std::cout << " - DTO-only presentation state\n";
        std::cout << " - typewriter queue and cursor state\n";
        std::cout << " - error severity preserved\n";
        std::cout << " - hidden intervals preserve scrolling diagnostic history\n";
        std::cout << " - ship/physics/activity streams append as terminal events\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "DOCKING STATUS TERMINAL: FAIL: "
            << e.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
