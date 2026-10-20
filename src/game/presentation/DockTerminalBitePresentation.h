#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "src/game/bite/BiteTypes.h"
#include "src/game/presentation/DockPlannerTerminalTelemetry.h"

namespace game::presentation
{

// The BITE domain returns typed faults; this presentation adapter owns their
// translated cockpit wording. Internal equipment IDs remain stable diagnostics
// and are not incorrectly translated as free-form sentences.
class DockTerminalBitePresentation final
{
public:
    using Lookup = TerminalLocalizedText::Lookup;

    [[nodiscard]] static std::vector<std::string> lines(
        const game::bite::State& state,
        const Lookup& lookup
    )
    {
        std::vector<std::string> result;
        result.reserve(state.faults.size());

        for (const auto& fault : state.faults)
        {
            const char* severityKey = "cockpit.terminal.bite.advisory";
            switch (fault.severity)
            {
                case game::bite::Severity::Advisory: break;
                case game::bite::Severity::Caution:
                    severityKey = "cockpit.terminal.bite.caution"; break;
                case game::bite::Severity::Warning:
                    severityKey = "cockpit.terminal.bite.warning"; break;
                case game::bite::Severity::Failed:
                    severityKey = "cockpit.terminal.bite.failed"; break;
            }

            std::string detail;
            switch (fault.kind)
            {
                case game::bite::FaultKind::EquipmentFailed:
                    detail = lookup("cockpit.terminal.bite.inoperative");
                    break;
                case game::bite::FaultKind::FunctionalDegradation:
                    detail = TerminalLocalizedText::render(
                        lookup, "cockpit.terminal.bite.output",
                        {percent(fault.functionalAuthority01)});
                    break;
                case game::bite::FaultKind::NozzleDegradation:
                    detail = TerminalLocalizedText::render(
                        lookup, "cockpit.terminal.bite.nozzle",
                        {percent(fault.secondaryAuthority01)});
                    break;
                case game::bite::FaultKind::MaintenanceRequired:
                    detail = TerminalLocalizedText::render(
                        lookup, "cockpit.terminal.bite.maintenance",
                        {percent(fault.wear01)});
                    break;
            }

            std::string description = TerminalLocalizedText::render(
                lookup, "cockpit.terminal.bite.fault",
                {fault.displayName, lookup(severityKey), detail});

            if (fault.powerIncrease01 > 0.0)
                description += TerminalLocalizedText::render(
                    lookup, "cockpit.terminal.bite.power",
                    {percent(fault.powerIncrease01)});
            if (fault.heatIncrease01 > 0.0)
                description += TerminalLocalizedText::render(
                    lookup, "cockpit.terminal.bite.heat",
                    {percent(fault.heatIncrease01)});
            if (fault.outputInstability01 > 0.0)
                description += TerminalLocalizedText::render(
                    lookup, "cockpit.terminal.bite.instability",
                    {percent(fault.outputInstability01)});

            result.push_back(std::move(description));
        }

        return result;
    }

private:
    [[nodiscard]] static std::string percent(double fraction)
    {
        const double safe = std::isfinite(fraction)
            ? std::clamp(fraction, 0.0, 1.0) : 0.0;
        return TerminalLocalizedText::number(safe * 100.0, 0);
    }
};

} // namespace game::presentation
