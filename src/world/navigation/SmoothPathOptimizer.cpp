#include "src/world/navigation/SmoothPathOptimizer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <utility>

#include "src/world/navigation/NavigationObstacleGeometry.h"

namespace world::navigation
{
namespace
{
constexpr double Epsilon = 1.0e-9;
constexpr int MaxAdaptiveDepth = 18;

using PerfClock = std::chrono::steady_clock;

double elapsedMs(const PerfClock::time_point& begin)
{
    return std::chrono::duration<double, std::milli>(
        PerfClock::now() - begin
    ).count();
}

struct CandidatePerf
{
    std::size_t level = 0;
    std::size_t controlCount = 0;
    std::size_t sampleCount = 0;
    double sampleMs = 0.0;
    double safetyMs = 0.0;
    double qualityMs = 0.0;
    bool safe = false;
    bool curvatureAccepted = false;
};

void appendNavigationPerf(
    const SmoothPathRequest& request,
    const SmoothPathResult& result,
    double totalMs,
    const std::vector<CandidatePerf>& candidates,
    double fallbackSampleMs = 0.0,
    double fallbackSafetyMs = 0.0,
    std::size_t fallbackSamples = 0
)
{
    static std::mutex mutex;
    static std::ofstream out("navigation_perf.log", std::ios::app);
    if (!out)
        return;

    std::lock_guard<std::mutex> lock(mutex);
    out << std::fixed << std::setprecision(4)
        << "[SmoothPathPerf] total_ms=" << totalMs
        << " path_points=" << request.pathPointsMeters.size()
        << " obstacles=" << request.obstacles.size()
        << " max_support=" << request.maxSupportLevel
        << " spacing_m=" << request.maxSampleSpacingMeters
        << " chord_error_m=" << request.maxChordErrorMeters
        << " candidates=" << result.diagnostics.candidatesEvaluated
        << " safe_candidates=" << result.diagnostics.safeCandidates
        << " selected_support=" << result.diagnostics.selectedSupportLevel
        << " output_samples=" << result.points.size()
        << " fallback=" << (result.diagnostics.fellBackToPolyline ? 1 : 0)
        << " valid=" << (result.valid ? 1 : 0)
        << '\n';

    for (const CandidatePerf& candidate : candidates)
    {
        out << std::fixed << std::setprecision(4)
            << "[SmoothCandidatePerf] level=" << candidate.level
            << " controls=" << candidate.controlCount
            << " samples=" << candidate.sampleCount
            << " sample_ms=" << candidate.sampleMs
            << " safety_ms=" << candidate.safetyMs
            << " quality_ms=" << candidate.qualityMs
            << " safe=" << (candidate.safe ? 1 : 0)
            << " curvature_ok=" << (candidate.curvatureAccepted ? 1 : 0)
            << '\n';
    }

    if (fallbackSamples > 0)
    {
        out << std::fixed << std::setprecision(4)
            << "[SmoothFallbackPerf] samples=" << fallbackSamples
            << " sample_ms=" << fallbackSampleMs
            << " safety_ms=" << fallbackSafetyMs
            << '\n';
    }
}

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite3(const glm::dvec3& value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

double polylineLength(const std::vector<glm::dvec3>& points)
{
    double total = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i)
        total += glm::length(points[i] - points[i - 1]);
    return total;
}

std::vector<double> coarseProgress(const std::vector<glm::dvec3>& points)
{
    std::vector<double> out(points.size(), 0.0);
    for (std::size_t i = 1; i < points.size(); ++i)
        out[i] = out[i - 1] + glm::length(points[i] - points[i - 1]);
    return out;
}

struct ControlPoint
{
    glm::dvec3 positionMeters {0.0};
    double sourceProgressMeters = 0.0;
};

std::vector<ControlPoint> makeControls(
    const std::vector<glm::dvec3>& raw,
    const std::vector<double>& progress,
    std::size_t supportLevel
)
{
    std::vector<ControlPoint> controls;
    if (raw.empty())
        return controls;

    const std::size_t subdivisions = std::size_t(1) << supportLevel;
    controls.push_back({raw.front(), progress.front()});
    for (std::size_t i = 1; i < raw.size(); ++i)
    {
        for (std::size_t part = 1; part <= subdivisions; ++part)
        {
            const double u = static_cast<double>(part) /
                static_cast<double>(subdivisions);
            controls.push_back({
                raw[i - 1] + (raw[i] - raw[i - 1]) * u,
                progress[i - 1] + (progress[i] - progress[i - 1]) * u
            });
        }
    }

    // Cubic B-spline needs four controls. A two/three-point route remains a
    // straight/planar curve by inserting controls on the same segments.
    while (controls.size() < 4 && controls.size() >= 2)
    {
        std::vector<ControlPoint> expanded;
        expanded.reserve(controls.size() * 2 - 1);
        for (std::size_t i = 1; i < controls.size(); ++i)
        {
            expanded.push_back(controls[i - 1]);
            expanded.push_back({
                (controls[i - 1].positionMeters + controls[i].positionMeters) * 0.5,
                (controls[i - 1].sourceProgressMeters + controls[i].sourceProgressMeters) * 0.5
            });
        }
        expanded.push_back(controls.back());
        controls = std::move(expanded);
    }
    return controls;
}

std::vector<double> openUniformKnots(std::size_t controlCount, int degree)
{
    const std::size_t n = controlCount - 1;
    const std::size_t knotCount = n + static_cast<std::size_t>(degree) + 2;
    std::vector<double> knots(knotCount, 0.0);
    const std::size_t last = knotCount - 1;
    for (std::size_t i = 0; i < knotCount; ++i)
    {
        if (i <= static_cast<std::size_t>(degree))
            knots[i] = 0.0;
        else if (i >= last - static_cast<std::size_t>(degree))
            knots[i] = 1.0;
        else
        {
            const std::size_t interiorIndex = i - static_cast<std::size_t>(degree);
            const std::size_t interiorCount =
                controlCount - static_cast<std::size_t>(degree);
            knots[i] = static_cast<double>(interiorIndex) /
                static_cast<double>(interiorCount);
        }
    }
    return knots;
}

std::size_t findSpan(
    double u,
    const std::vector<double>& knots,
    std::size_t controlCount,
    int degree
)
{
    const std::size_t n = controlCount - 1;
    if (u >= 1.0 - Epsilon)
        return n;

    const std::size_t p = static_cast<std::size_t>(degree);

    // The previous implementation linearly scanned every knot for every
    // adaptive spline evaluation. With support level 5 this turns thousands
    // of samples into millions of avoidable comparisons. upper_bound preserves
    // the exact half-open span semantics (including values exactly on an
    // interior knot) while reducing lookup to O(log controls).
    const auto it = std::upper_bound(knots.begin(), knots.end(), u);
    if (it == knots.begin())
        return p;

    const std::size_t raw = static_cast<std::size_t>(
        std::distance(knots.begin(), it) - 1
    );
    return std::clamp(raw, p, n);
}

SmoothPathPoint evaluateSpline(
    const std::vector<ControlPoint>& controls,
    const std::vector<double>& knots,
    double u
)
{
    constexpr int Degree = 3;
    const std::size_t span = findSpan(u, knots, controls.size(), Degree);
    ControlPoint d[Degree + 1];
    for (int j = 0; j <= Degree; ++j)
        d[j] = controls[span - static_cast<std::size_t>(Degree) + static_cast<std::size_t>(j)];

    for (int r = 1; r <= Degree; ++r)
    {
        for (int j = Degree; j >= r; --j)
        {
            const std::size_t i = span - static_cast<std::size_t>(Degree) +
                static_cast<std::size_t>(j);
            const double denominator =
                knots[i + static_cast<std::size_t>(Degree - r + 1)] - knots[i];
            const double alpha = denominator > Epsilon
                ? std::clamp((u - knots[i]) / denominator, 0.0, 1.0)
                : 0.0;
            d[j].positionMeters = d[j - 1].positionMeters * (1.0 - alpha) +
                d[j].positionMeters * alpha;
            d[j].sourceProgressMeters =
                d[j - 1].sourceProgressMeters * (1.0 - alpha) +
                d[j].sourceProgressMeters * alpha;
        }
    }
    return {d[Degree].positionMeters, d[Degree].sourceProgressMeters};
}

double distancePointToSegment(
    const glm::dvec3& point,
    const glm::dvec3& a,
    const glm::dvec3& b
)
{
    const glm::dvec3 segment = b - a;
    const double length2 = glm::dot(segment, segment);
    if (length2 <= Epsilon)
        return glm::length(point - a);
    const double t = std::clamp(
        glm::dot(point - a, segment) / length2,
        0.0,
        1.0
    );
    return glm::length(point - (a + segment * t));
}

void adaptiveSample(
    const std::vector<ControlPoint>& controls,
    const std::vector<double>& knots,
    double u0,
    const SmoothPathPoint& p0,
    double u1,
    const SmoothPathPoint& p1,
    double maxSpacing,
    double maxError,
    int depth,
    std::vector<SmoothPathPoint>& out
)
{
    const double um = (u0 + u1) * 0.5;
    const SmoothPathPoint pm = evaluateSpline(controls, knots, um);
    const double chord = glm::length(p1.positionMeters - p0.positionMeters);
    const double error = distancePointToSegment(
        pm.positionMeters,
        p0.positionMeters,
        p1.positionMeters
    );
    if (depth < MaxAdaptiveDepth &&
        (chord > maxSpacing || error > maxError))
    {
        adaptiveSample(
            controls, knots, u0, p0, um, pm,
            maxSpacing, maxError, depth + 1, out
        );
        adaptiveSample(
            controls, knots, um, pm, u1, p1,
            maxSpacing, maxError, depth + 1, out
        );
        return;
    }
    if (out.empty() ||
        glm::length(out.back().positionMeters - p1.positionMeters) > 1.0e-8)
    {
        out.push_back(p1);
    }
}

std::vector<SmoothPathPoint> sampleSpline(
    const std::vector<ControlPoint>& controls,
    double maxSpacing,
    double maxError
)
{
    if (controls.size() < 4)
        return {};
    const auto knots = openUniformKnots(controls.size(), 3);
    std::vector<SmoothPathPoint> out;
    const auto first = evaluateSpline(controls, knots, 0.0);
    const auto last = evaluateSpline(controls, knots, 1.0);
    out.push_back(first);
    adaptiveSample(
        controls, knots, 0.0, first, 1.0, last,
        maxSpacing, maxError, 0, out
    );
    if (!out.empty())
    {
        out.front() = {controls.front().positionMeters, controls.front().sourceProgressMeters};
        out.back() = {controls.back().positionMeters, controls.back().sourceProgressMeters};
    }
    return out;
}

bool segmentSafe(
    const SmoothPathPoint& a,
    const SmoothPathPoint& b,
    const SmoothPathRequest& request
)
{
    for (const auto& obstacle : request.obstacles)
    {
        if (!obstacle.finite())
            continue;
        const bool terminalException =
            !request.terminalAllowedObstacleId.empty() &&
            obstacle.id == request.terminalAllowedObstacleId &&
            a.sourceProgressMeters >=
                request.terminalObstacleEntrySourceProgressMeters - 1.0e-7 &&
            b.sourceProgressMeters >=
                request.terminalObstacleEntrySourceProgressMeters - 1.0e-7;
        if (terminalException)
            continue;
        if (segmentIntersectsNavigationObstacle(
                a.positionMeters,
                b.positionMeters,
                obstacle,
                request.vehicle.collisionRadiusMeters,
                request.vehicle.preferredClearanceMeters))
        {
            return false;
        }
    }
    return true;
}

bool pathSafe(
    const std::vector<SmoothPathPoint>& points,
    const SmoothPathRequest& request
)
{
    if (points.size() < 2)
        return false;
    for (std::size_t i = 1; i < points.size(); ++i)
    {
        if (!segmentSafe(points[i - 1], points[i], request))
            return false;
    }
    return true;
}

struct CurveQuality
{
    double lengthMeters = 0.0;
    double maxCurvature = 0.0;
    double curvatureVariation = 0.0;
    double score = std::numeric_limits<double>::infinity();
};

CurveQuality quality(const std::vector<SmoothPathPoint>& points)
{
    CurveQuality q;
    if (points.size() < 2)
        return q;

    std::vector<double> curvature(points.size(), 0.0);
    for (std::size_t i = 1; i < points.size(); ++i)
        q.lengthMeters += glm::length(points[i].positionMeters - points[i - 1].positionMeters);

    for (std::size_t i = 1; i + 1 < points.size(); ++i)
    {
        const glm::dvec3 a = points[i].positionMeters - points[i - 1].positionMeters;
        const glm::dvec3 b = points[i + 1].positionMeters - points[i].positionMeters;
        const double la = glm::length(a);
        const double lb = glm::length(b);
        if (la <= Epsilon || lb <= Epsilon)
            continue;
        const double angle = std::acos(std::clamp(
            glm::dot(a / la, b / lb), -1.0, 1.0
        ));
        curvature[i] = angle / std::max(Epsilon, 0.5 * (la + lb));
        q.maxCurvature = std::max(q.maxCurvature, curvature[i]);
    }
    for (std::size_t i = 2; i + 1 < points.size(); ++i)
        q.curvatureVariation += std::abs(curvature[i] - curvature[i - 1]);

    // Smoothness dominates distance deliberately. A longer broad approach is
    // cheaper than a short route that demands a visually abrupt turn.
    q.score = q.maxCurvature * 1.0e8 +
        q.curvatureVariation * 1.0e7 +
        q.lengthMeters * 1.0e-4;
    return q;
}

std::vector<SmoothPathPoint> rawPolyline(
    const std::vector<glm::dvec3>& raw,
    const std::vector<double>& progress,
    double spacing
)
{
    std::vector<SmoothPathPoint> out;
    if (raw.empty())
        return out;
    out.push_back({raw.front(), progress.front()});
    for (std::size_t i = 1; i < raw.size(); ++i)
    {
        const double length = glm::length(raw[i] - raw[i - 1]);
        const std::size_t pieces = std::max<std::size_t>(
            1,
            static_cast<std::size_t>(std::ceil(length / spacing))
        );
        for (std::size_t part = 1; part <= pieces; ++part)
        {
            const double u = static_cast<double>(part) / static_cast<double>(pieces);
            out.push_back({
                raw[i - 1] + (raw[i] - raw[i - 1]) * u,
                progress[i - 1] + (progress[i] - progress[i - 1]) * u
            });
        }
    }
    return out;
}

bool validRequest(const SmoothPathRequest& request)
{
    if (request.pathPointsMeters.size() < 2 ||
        (!request.obstacles.empty() && !request.vehicle.valid()) ||
        !finite(request.maxSampleSpacingMeters) || request.maxSampleSpacingMeters <= 0.0 ||
        !finite(request.maxChordErrorMeters) || request.maxChordErrorMeters <= 0.0 ||
        !finite(request.maxCurvaturePerMeter) || request.maxCurvaturePerMeter < 0.0)
    {
        return false;
    }
    for (const auto& point : request.pathPointsMeters)
    {
        if (!finite3(point))
            return false;
    }
    return true;
}

} // namespace

SmoothPathResult SmoothPathOptimizer::optimize(const SmoothPathRequest& request)
{
    const auto totalBegin = PerfClock::now();
    SmoothPathResult out;
    if (!validRequest(request))
    {
        out.message = "invalid smooth path request";
        appendNavigationPerf(request, out, elapsedMs(totalBegin), {});
        return out;
    }

    std::vector<double> progress;
    if (request.sourceProgressMeters.size() == request.pathPointsMeters.size())
    {
        progress = request.sourceProgressMeters;
        bool monotone = !progress.empty() && finite(progress.front());
        for (std::size_t i = 1; monotone && i < progress.size(); ++i)
            monotone = finite(progress[i]) && progress[i] + Epsilon >= progress[i - 1];
        if (!monotone)
            progress.clear();
    }
    if (progress.empty())
        progress = coarseProgress(request.pathPointsMeters);
    out.diagnostics.coarseLengthMeters = polylineLength(request.pathPointsMeters);
    if (!finite(out.diagnostics.coarseLengthMeters) ||
        out.diagnostics.coarseLengthMeters <= Epsilon)
    {
        out.message = "smooth path source has zero length";
        appendNavigationPerf(request, out, elapsedMs(totalBegin), {});
        return out;
    }

    std::vector<CandidatePerf> candidatePerf;
    candidatePerf.reserve(request.maxSupportLevel + 1);

    double bestScore = std::numeric_limits<double>::infinity();
    for (std::size_t level = 0; level <= request.maxSupportLevel; ++level)
    {
        ++out.diagnostics.candidatesEvaluated;
        CandidatePerf perf;
        perf.level = level;

        const auto controls = makeControls(request.pathPointsMeters, progress, level);
        perf.controlCount = controls.size();

        const auto sampleBegin = PerfClock::now();
        const auto candidate = sampleSpline(
            controls,
            std::max(0.5, request.maxSampleSpacingMeters),
            std::max(1.0e-4, request.maxChordErrorMeters)
        );
        perf.sampleMs = elapsedMs(sampleBegin);
        perf.sampleCount = candidate.size();

        const auto safetyBegin = PerfClock::now();
        perf.safe = pathSafe(candidate, request);
        perf.safetyMs = elapsedMs(safetyBegin);
        if (!perf.safe)
        {
            candidatePerf.push_back(perf);
            continue;
        }

        const auto qualityBegin = PerfClock::now();
        const CurveQuality q = quality(candidate);
        perf.qualityMs = elapsedMs(qualityBegin);
        perf.curvatureAccepted =
            request.maxCurvaturePerMeter <= 0.0 ||
            q.maxCurvature <= request.maxCurvaturePerMeter + 1.0e-10;
        if (!perf.curvatureAccepted)
        {
            candidatePerf.push_back(perf);
            continue;
        }

        ++out.diagnostics.safeCandidates;
        if (q.score < bestScore)
        {
            bestScore = q.score;
            out.points = candidate;
            out.diagnostics.selectedSupportLevel = level;
            out.diagnostics.optimizedLengthMeters = q.lengthMeters;
            out.diagnostics.maxCurvaturePerMeter = q.maxCurvature;
            out.diagnostics.curvatureVariation = q.curvatureVariation;
        }
        candidatePerf.push_back(perf);
    }

    if (!out.points.empty())
    {
        out.valid = true;
        out.message = "collision-free global cubic B-spline path";
        appendNavigationPerf(
            request,
            out,
            elapsedMs(totalBegin),
            candidatePerf
        );
        return out;
    }

    // Trajectory generation may retain a known-safe coarse topology. Manual
    // guidance explicitly disables this: a kinked polyline is not a usable
    // pilot corridor even when it is collision-free.
    double fallbackSampleMs = 0.0;
    double fallbackSafetyMs = 0.0;
    std::size_t fallbackSamples = 0;
    if (request.allowPolylineFallback)
    {
        const auto fallbackSampleBegin = PerfClock::now();
        auto fallback = rawPolyline(
            request.pathPointsMeters,
            progress,
            std::max(0.5, request.maxSampleSpacingMeters)
        );
        fallbackSampleMs = elapsedMs(fallbackSampleBegin);
        fallbackSamples = fallback.size();

        const auto fallbackSafetyBegin = PerfClock::now();
        const bool fallbackSafe = pathSafe(fallback, request);
        fallbackSafetyMs = elapsedMs(fallbackSafetyBegin);
        if (fallbackSafe)
        {
            const CurveQuality q = quality(fallback);
            const bool curvatureOk = request.maxCurvaturePerMeter <= 0.0 ||
                q.maxCurvature <= request.maxCurvaturePerMeter + 1.0e-10;
            if (curvatureOk)
            {
                out.points = std::move(fallback);
                out.diagnostics.optimizedLengthMeters = q.lengthMeters;
                out.diagnostics.maxCurvaturePerMeter = q.maxCurvature;
                out.diagnostics.curvatureVariation = q.curvatureVariation;
                out.diagnostics.fellBackToPolyline = true;
                out.valid = true;
                out.message = "safe coarse path fallback";
                appendNavigationPerf(
                    request,
                    out,
                    elapsedMs(totalBegin),
                    candidatePerf,
                    fallbackSampleMs,
                    fallbackSafetyMs,
                    fallbackSamples
                );
                return out;
            }
        }
    }

    out.message = request.maxCurvaturePerMeter > 0.0
        ? "no collision-free curve within curvature bound"
        : "no collision-free global smooth path";
    appendNavigationPerf(
        request,
        out,
        elapsedMs(totalBegin),
        candidatePerf,
        fallbackSampleMs,
        fallbackSafetyMs,
        fallbackSamples
    );
    return out;
}

} // namespace world::navigation
