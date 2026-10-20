#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include "src/game/presentation/DockTerminalBitePresentation.h"

#include "src/game/presentation/DockingStatusTerminal.h"
#include "src/game/presentation/DockPlannerTerminalTelemetry.h"

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

        using game::presentation::DockPlannerTerminalTelemetry;
        using game::navigation::planner::AssistedRoutePreset;
        using game::navigation::planner::RoutePlanRequest;

        const game::presentation::TerminalLocalizedText::Lookup en =
            [](const char* key) {
                static const std::unordered_map<std::string, std::string> translations {
                    {"cockpit.terminal.preset.extreme", "EXTREME"},
                    {"cockpit.terminal.preset_line", "MODE {0}"},
                    {"cockpit.terminal.metric.elapsed", "T={0}s V0={1} MAX={2}m/s"},
                    {"cockpit.terminal.metric.limits", "ACC={0} BRK={1} LAT={2}"},
                    {"cockpit.terminal.metric.solve", "SOLVE IN={0}s OUT={1}s"},
                    {"cockpit.terminal.metric.eta", "ETA IN={0}s OUT={1}s"},
                    {"cockpit.terminal.metric.evaluations", "EVAL IN={0} OUT={1}"},
                    {"cockpit.terminal.bite.failed", "FAILED"},
                    {"cockpit.terminal.bite.inoperative", "equipment inoperative"},
                    {"cockpit.terminal.bite.fault", "{0} · {1}: {2}"}
                };
                return translations.at(key);
            };
        const game::presentation::TerminalLocalizedText::Lookup ru =
            [](const char* key) {
                static const std::unordered_map<std::string, std::string> translations {
                    {"cockpit.terminal.preset.extreme", "ЭКСТРИМ"},
                    {"cockpit.terminal.preset_line", "РЕЖИМ {0}"}
                };
                return translations.at(key);
            };

        RoutePlanRequest request;
        request.assistedRoutePreset = AssistedRoutePreset::Extreme;
        request.initialSpeedMps = 0.0;
        request.maxSpeedMps = 500.0;
        request.acceleratingMps2 = 19.2;
        request.brakingMps2 = 17.652;
        request.lateralMps2 = 17.652;
        require(
            DockPlannerTerminalTelemetry::presetLine(en, request.assistedRoutePreset).find("EXTREME") !=
                std::string::npos,
            "terminal did not expose actual experiment preset"
        );
        require(
            DockPlannerTerminalTelemetry::elapsed(en, 6.9, request).find("T=6s") !=
                std::string::npos,
            "terminal displayed fabricated or unbucketed worker duration"
        );
        require(
            DockPlannerTerminalTelemetry::limits(en, request).find("BRK=17.7") !=
                std::string::npos,
            "terminal did not use real braking authority"
        );

        game::navigation::planner::RoutePlan plan;
        plan.diagnosticSummary =
            "segmented route: inbound_ms=5300.94 inbound_detail={ "
            "continuous-assisted ready evals=4682 eta_s=101.129 } "
            "final_ms=5678.55 final_detail={ "
            "continuous-assisted ready evals=4726 eta_s=75.745 }";
        require(
            DockPlannerTerminalTelemetry::legTimings(en, plan) ==
                "SOLVE IN=5.30s OUT=5.68s",
            "terminal did not render measured per-leg solve times"
        );
        require(
            DockPlannerTerminalTelemetry::legEta(en, plan) ==
                "ETA IN=101.1s OUT=75.7s",
            "terminal did not render real per-leg ETA"
        );
        require(
            DockPlannerTerminalTelemetry::legEvaluations(en, plan) ==
                "EVAL IN=4682 OUT=4726",
            "terminal did not render measured evaluation counts"
        );

        require(
            DockPlannerTerminalTelemetry::presetLine(
                ru, request.assistedRoutePreset) == "РЕЖИМ ЭКСТРИМ",
            "localized Russian experiment preset lost"
        );
        game::bite::State faults;
        game::bite::Fault fault;
        fault.displayName = "ENGINE_A";
        fault.kind = game::bite::FaultKind::EquipmentFailed;
        fault.severity = game::bite::Severity::Failed;
        faults.faults.push_back(fault);
        const auto localizedBite =
            game::presentation::DockTerminalBitePresentation::lines(faults, en);
        require(localizedBite.size() == 1 &&
                localizedBite.front().find("equipment inoperative") != std::string::npos,
                "BITE descriptions must be localized at UI boundary");

        DockingStatusTerminal terminal;

        DockingTerminalSnapshot snapshot;
        snapshot.visible = true;
        snapshot.localeId = "en";
        snapshot.title = "DOCKING";
        snapshot.biteNominalText = "BITE NOMINAL";
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
        snapshot.activity = DockPlannerTerminalTelemetry::elapsed(en, 6.9, request);
        snapshot.plannerDetails.push_back(
            DockPlannerTerminalTelemetry::limits(en, request)
        );
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

        // No old-language history may survive a language switch.
        snapshot.localeId = "ru";
        snapshot.title = "СТЫКОВКА";
        out = terminal.update(snapshot, 24.0);
        for (const auto& line : out.lines)
            require(line.text.find("DOCKING") == std::string::npos,
                    "language switch retained English terminal history");

        std::cout << "DOCKING STATUS TERMINAL: PASS\n";
        std::cout << " - DTO-only presentation state\n";
        std::cout << " - typewriter queue and cursor state\n";
        std::cout << " - error severity preserved\n";
        std::cout << " - hidden intervals preserve scrolling diagnostic history\n";
        std::cout << " - ship/physics/activity streams append as terminal events\n";        std::cout << " - real planner input, ETA and solve-time formatting\n";
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
