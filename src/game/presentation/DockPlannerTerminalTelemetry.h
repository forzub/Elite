#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <string>

#include "src/game/navigation/planner/RoutePlannerApi.h"

namespace game::presentation
{

// One client-owned localization entry point. The text of EVERY terminal
// label/template comes from the active localization catalog; numerical values
// and stable internal IDs are data, not translatable literals.
class TerminalLocalizedText final
{
public:
    using Lookup = std::function<std::string(const char*)>;

    [[nodiscard]] static std::string render(
        const Lookup& lookup,
        const char* key,
        std::initializer_list<std::string> args = {}
    )
    {
        std::string line = lookup(key);
        std::size_t index = 0;
        for (const auto& value : args)
        {
            const std::string placeholder =
                "{" + std::to_string(index++) + "}";
            std::size_t pos = 0;
            while ((pos = line.find(placeholder, pos)) !=
                   std::string::npos)
            {
                line.replace(pos, placeholder.size(), value);
                pos += value.size();
            }
        }
        return line;
    }

    [[nodiscard]] static std::string number(double value, int digits = 0)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(digits) << value;
        return out.str();
    }
};

class DockPlannerTerminalTelemetry final
{
public:
    using Request = game::navigation::planner::RoutePlanRequest;
    using Plan = game::navigation::planner::RoutePlan;
    using Lookup = TerminalLocalizedText::Lookup;

    // Map internal diagnostic reasons to localized operator categories.
    // The original full technical string remains in the existing log.
    [[nodiscard]] static std::string failure(
        const Lookup& lookup,
        const std::string& technicalReason
    )
    {
        const auto has = [&technicalReason](const char* token)
        {
            return technicalReason.find(token) != std::string::npos;
        };
        const char* key = "cockpit.terminal.failure_generic";
        if (has("hull does not fit") || has("dock unavailable"))
            key = "cockpit.terminal.failure_dock";
        else if (has("motor telemetry") || has("motor authority"))
            key = "cockpit.terminal.failure_telemetry";
        else if (has("motors cannot") || has("motors degraded") ||
                 has("propulsion"))
            key = "cockpit.terminal.failure_propulsion";
        else if (has("KeepInside") || has("blue lane") ||
                 has("blue transit") || has("BLUE"))
            key = "cockpit.terminal.failure_blue";
        else if (has("traffic graph") || has("navigation volume"))
            key = "cockpit.terminal.failure_traffic";
        else if (has("handoff") || has("connector"))
            key = "cockpit.terminal.failure_handoff";
        else if (has("safe-course"))
            key = "cockpit.terminal.failure_safety";
        else if (has("snapshot") || has("authoritative ship state") ||
                 has("target Hub frame"))
            key = "cockpit.terminal.failure_snapshot";
        else if (has("dock local pose") || has("dock state"))
            key = "cockpit.terminal.failure_port";
        else if (has("planner") || has("route") || has("planning"))
            key = "cockpit.terminal.failure_route";
        return lookup(key);
    }

    [[nodiscard]] static const char* stageKey(std::uint8_t stage) noexcept
    {
        switch (stage)
        {
            case 0: return "cockpit.terminal.planner_stage.prepare";
            case 1: return "cockpit.terminal.planner_stage.geometry";
            case 2: return "cockpit.terminal.planner_stage.frames";
            case 3: return "cockpit.terminal.planner_stage.control";
            case 4: return "cockpit.terminal.planner_stage.complete";
            default: return "cockpit.terminal.planner_stage.unknown";
        }
    }

    [[nodiscard]] static std::string presetName(
        const Lookup& lookup,
        game::navigation::planner::AssistedRoutePreset preset
    )
    {
        return lookup(preset ==
            game::navigation::planner::AssistedRoutePreset::Extreme
                ? "cockpit.terminal.preset.extreme"
                : "cockpit.terminal.preset.standard");
    }

    [[nodiscard]] static std::string presetLine(
        const Lookup& lookup,
        game::navigation::planner::AssistedRoutePreset preset
    )
    {
        return TerminalLocalizedText::render(
            lookup, "cockpit.terminal.preset_line",
            {presetName(lookup, preset)});
    }

    [[nodiscard]] static std::string running(
        const Lookup& lookup,
        std::uint8_t stage,
        int activeWorkers
    )
    {
        return TerminalLocalizedText::render(
            lookup, "cockpit.terminal.metric.planning",
            {lookup(stageKey(stage)),
             std::to_string(std::max(0, activeWorkers))});
    }

    [[nodiscard]] static std::string elapsed(
        const Lookup& lookup,
        double elapsedWallSeconds,
        const Request& request
    )
    {
        // Actual monotonic runtime, quantized only to avoid typewriter spam.
        const int seconds = static_cast<int>(
            std::clamp(
                std::floor(std::max(0.0, elapsedWallSeconds) / 3.0) * 3.0,
                0.0, 999999.0)
        );
        return TerminalLocalizedText::render(
            lookup, "cockpit.terminal.metric.elapsed",
            {std::to_string(seconds),
             TerminalLocalizedText::number(request.initialSpeedMps),
             TerminalLocalizedText::number(request.maxSpeedMps)});
    }

    [[nodiscard]] static std::string limits(
        const Lookup& lookup,
        const Request& request
    )
    {
        return TerminalLocalizedText::render(
            lookup, "cockpit.terminal.metric.limits",
            {TerminalLocalizedText::number(request.acceleratingMps2, 1),
             TerminalLocalizedText::number(request.brakingMps2, 1),
             TerminalLocalizedText::number(request.lateralMps2, 1)});
    }

    [[nodiscard]] static std::string obstacles(
        const Lookup& lookup,
        const Request& request
    )
    {
        return TerminalLocalizedText::render(
            lookup, "cockpit.terminal.metric.obstacles",
            {std::to_string(request.obstacles.size())});
    }

    [[nodiscard]] static std::string completed(
        const Lookup& lookup,
        double workerMs
    )
    {
        return TerminalLocalizedText::render(
            lookup, "cockpit.terminal.metric.completed",
            {TerminalLocalizedText::number(
                std::max(0.0, workerMs / 1000.0), 2)});
    }

    [[nodiscard]] static std::string route(
        const Lookup& lookup,
        const Plan& plan
    )
    {
        const double length = plan.routeCurves.empty() ? 0.0 :
            plan.routeCurves.back().endProgressMeters -
            plan.routeCurves.front().startProgressMeters;
        return TerminalLocalizedText::render(
            lookup, "cockpit.terminal.metric.route",
            {TerminalLocalizedText::number(length / 1000.0, 2),
             std::to_string(plan.routeCurves.size())});
    }

    [[nodiscard]] static std::string legTimings(
        const Lookup& lookup,
        const Plan& plan
    )
    {
        const double inbound = readNumber(plan.diagnosticSummary, "inbound_ms=");
        const double last = readNumber(plan.diagnosticSummary, "final_ms=");
        if (!std::isfinite(inbound) || !std::isfinite(last))
            return {};
        return TerminalLocalizedText::render(
            lookup, "cockpit.terminal.metric.solve",
            {TerminalLocalizedText::number(inbound / 1000.0, 2),
             TerminalLocalizedText::number(last / 1000.0, 2)});
    }

    [[nodiscard]] static std::string legEta(
        const Lookup& lookup,
        const Plan& plan
    )
    {
        const double inbound = readInside(
            plan.diagnosticSummary, "inbound_detail={", "eta_s=");
        const double last = readInside(
            plan.diagnosticSummary, "final_detail={", "eta_s=");
        if (!std::isfinite(inbound) || !std::isfinite(last))
            return {};
        return TerminalLocalizedText::render(
            lookup, "cockpit.terminal.metric.eta",
            {TerminalLocalizedText::number(inbound, 1),
             TerminalLocalizedText::number(last, 1)});
    }

    [[nodiscard]] static std::string legEvaluations(
        const Lookup& lookup,
        const Plan& plan
    )
    {
        const double inbound = readInside(
            plan.diagnosticSummary, "inbound_detail={", "evals=");
        const double last = readInside(
            plan.diagnosticSummary, "final_detail={", "evals=");
        if (!std::isfinite(inbound) || !std::isfinite(last))
            return {};
        return TerminalLocalizedText::render(
            lookup, "cockpit.terminal.metric.evaluations",
            {std::to_string(static_cast<unsigned long long>(inbound)),
             std::to_string(static_cast<unsigned long long>(last))});
    }

private:
    [[nodiscard]] static double readNumber(
        const std::string& text,
        const std::string& token,
        std::size_t from = 0,
        std::size_t before = std::string::npos
    )
    {
        const auto found = text.find(token, from);
        if (found == std::string::npos ||
            (before != std::string::npos && found >= before))
            return std::numeric_limits<double>::quiet_NaN();
        const char* first = text.c_str() + found + token.size();
        char* end = nullptr;
        const double value = std::strtod(first, &end);
        if (end == first || !std::isfinite(value))
            return std::numeric_limits<double>::quiet_NaN();
        return value;
    }

    [[nodiscard]] static double readInside(
        const std::string& text,
        const std::string& anchor,
        const std::string& field
    )
    {
        const auto begin = text.find(anchor);
        if (begin == std::string::npos)
            return std::numeric_limits<double>::quiet_NaN();
        const auto end = text.find(" }", begin + anchor.size());
        return readNumber(text, field, begin + anchor.size(), end);
    }
};

} // namespace game::presentation
