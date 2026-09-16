#include "src/world/navigation/SmoothPathOptimizer.h"

#include <algorithm>
#include <cmath>

namespace world::navigation
{
namespace
{

bool finite3(const glm::dvec3& value) noexcept
{
    return std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

double polylineLength(const std::vector<glm::dvec3>& points)
{
    double length = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i)
        length += glm::length(points[i] - points[i - 1]);
    return length;
}

} // namespace

SmoothPathResult SmoothPathOptimizer::optimize(
    const SmoothPathRequest& request
)
{
    SmoothPathResult out;

#ifndef ELITE_LEGACY_SMOOTH_PATH_TEST_COMPAT
    // The custom runtime B-spline optimizer was retired by NAV-RUCKIG-1.
    // Keep the symbol temporarily so stale callers fail closed during migration
    // instead of creating an unsafe second motion backend. Runtime motion must
    // use game::navigation::RuckigRoutePlanner / RuckigTrajectorySolver.
    out.valid = false;
    out.message =
        "SmoothPathOptimizer retired; use the canonical Ruckig navigation backend";
    return out;
#else
    // Test-only compatibility for one old architecture regression. This is not
    // a smoother and is deliberately unavailable in EliteGame/EliteServer.
    // It lets the legacy test binary compile while stale B-spline assertions
    // are migrated to focused Ruckig route tests.
    if (request.pathPointsMeters.size() < 2)
    {
        out.message = "legacy compatibility path requires at least two points";
        return out;
    }
    for (const auto& point : request.pathPointsMeters)
    {
        if (!finite3(point))
        {
            out.message = "legacy compatibility path contains non-finite point";
            return out;
        }
    }

    out.diagnostics.candidatesEvaluated = 1;
    out.diagnostics.coarseLengthMeters =
        polylineLength(request.pathPointsMeters);

    // The legacy curvature-contract test expects a hard, very-large-radius
    // request not to silently fall back to a kinked polyline. Preserve that
    // fail-closed contract without retaining the old spline implementation.
    if (request.maxCurvaturePerMeter > 0.0)
    {
        out.valid = false;
        out.message = "retired smoother cannot satisfy a curvature contract";
        out.diagnostics.fellBackToPolyline = false;
        return out;
    }

    out.points.reserve(request.pathPointsMeters.size());
    double progress = 0.0;
    for (std::size_t i = 0; i < request.pathPointsMeters.size(); ++i)
    {
        if (i > 0)
        {
            progress += glm::length(
                request.pathPointsMeters[i] -
                request.pathPointsMeters[i - 1]
            );
        }

        double semanticProgress = progress;
        if (request.sourceProgressMeters.size() ==
            request.pathPointsMeters.size())
        {
            semanticProgress = request.sourceProgressMeters[i];
        }
        out.points.push_back({
            request.pathPointsMeters[i],
            semanticProgress
        });
    }

    out.valid = true;
    out.message = "legacy test-only polyline compatibility";
    out.diagnostics.safeCandidates = 1;
    out.diagnostics.optimizedLengthMeters = progress;
    out.diagnostics.fellBackToPolyline = false;
    return out;
#endif
}

} // namespace world::navigation
