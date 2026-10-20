#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

#include "src/game/navigation/planner/RoutePlannerApi.h"

namespace game::presentation
{

// Formatting only. Every displayed number comes from a real worker request,
// monotonic elapsed time, or completed RoutePlan diagnostics. No fake progress
// percentages and no hash/random decorative CALC stream.
class DockPlannerTerminalTelemetry final
{
public:
    using Request = game::navigation::planner::RoutePlanRequest;
    using Plan = game::navigation::planner::RoutePlan;

    [[nodiscard]] static const char* phase(std::uint8_t stage) noexcept
    {
        switch (stage)
        {
            case 0: return "PREPARE";
            case 1: return "GEOMETRY";
            case 2: return "FRAMES";
            case 3: return "CONTROL";
            case 4: return "COMPLETE";
            default: return "UNKNOWN";
        }
    }

    [[nodiscard]] static std::string running(
        std::uint8_t stage,
        int activeWorkers
    )
    {
        return std::string("PLANNER ") + phase(stage) +
            " W=" + std::to_string(std::max(0, activeWorkers));
    }

    [[nodiscard]] static std::string elapsed(
        double elapsedWallSeconds,
        const Request& request
    )
    {
        // Bucket only UI updates, not values. This prevents the 38 cps
        // terminal typewriter from accumulating a new line each frame.
        const int seconds = static_cast<int>(
            std::clamp(std::floor(std::max(0.0, elapsedWallSeconds) / 3.0) * 3.0,
                       0.0, 999999.0)
        );
        std::ostringstream out;
        out << "T=" << seconds << "s"
            << " V0=" << std::fixed << std::setprecision(0)
            << request.initialSpeedMps
            << " MAX=" << request.maxSpeedMps << "m/s";
        return out.str();
    }

    [[nodiscard]] static std::string limits(const Request& request)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(1)
            << "ACC=" << request.acceleratingMps2
            << " BRK=" << request.brakingMps2
            << " LAT=" << request.lateralMps2;
        return out.str();
    }

    [[nodiscard]] static std::string preset(const Request& request)
    {
        return std::string("ROUTE MODE ") +
            game::navigation::planner::assistedRoutePresetName(
                request.assistedRoutePreset
            ) + " OBS=" + std::to_string(request.obstacles.size());
    }

    [[nodiscard]] static std::string completed(double actualWorkerMs)
    {
        std::ostringstream out;
        out << "PLANNER READY " << std::fixed << std::setprecision(2)
            << std::max(0.0, actualWorkerMs / 1000.0) << "s";
        return out.str();
    }

    [[nodiscard]] static std::string route(
        const Plan& plan
    )
    {
        const double length = plan.routeCurves.empty()
            ? 0.0
            : plan.routeCurves.back().endProgressMeters -
                plan.routeCurves.front().startProgressMeters;
        std::ostringstream out;
        out << "PATH=" << std::fixed << std::setprecision(2)
            << length / 1000.0 << "km"
            << " CURVES=" << plan.routeCurves.size();
        return out.str();
    }

    [[nodiscard]] static std::string legTimings(const Plan& plan)
    {
        const double inbound = readNumber(plan.diagnosticSummary, "inbound_ms=");
        const double finalLeg = readNumber(plan.diagnosticSummary, "final_ms=");
        if (!std::isfinite(inbound) || !std::isfinite(finalLeg))
            return {};
        std::ostringstream out;
        out << std::fixed << std::setprecision(2)
            << "SOLVE IN=" << inbound / 1000.0
            << "s OUT=" << finalLeg / 1000.0 << "s";
        return out.str();
    }

    [[nodiscard]] static std::string legEta(const Plan& plan)
    {
        const double inbound = readInside(
            plan.diagnosticSummary, "inbound_detail={", "eta_s="
        );
        const double finalLeg = readInside(
            plan.diagnosticSummary, "final_detail={", "eta_s="
        );
        if (!std::isfinite(inbound) || !std::isfinite(finalLeg))
            return {};
        std::ostringstream out;
        out << std::fixed << std::setprecision(1)
            << "ETA IN=" << inbound
            << "s OUT=" << finalLeg << "s";
        return out.str();
    }

    [[nodiscard]] static std::string legEvaluations(const Plan& plan)
    {
        const double inbound = readInside(
            plan.diagnosticSummary, "inbound_detail={", "evals="
        );
        const double finalLeg = readInside(
            plan.diagnosticSummary, "final_detail={", "evals="
        );
        if (!std::isfinite(inbound) || !std::isfinite(finalLeg))
            return {};
        std::ostringstream out;
        out << "EVAL IN=" << static_cast<unsigned long long>(inbound)
            << " OUT=" << static_cast<unsigned long long>(finalLeg);
        return out.str();
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
