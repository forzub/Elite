#include "src/game/navigation/traffic/RouteVolumeContainmentValidator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace game::navigation::traffic
{
namespace
{

constexpr double kNumericalInsetMeters = 1.0e-5;
constexpr double kMaxSampleSpacingMeters = 2.0;
constexpr std::size_t kMaxSamplesPerCurve = 20000;

bool finite3(const glm::dvec3& value) noexcept
{
    return
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

bool finitePositive(double value) noexcept
{
    return std::isfinite(value) && value > 0.0;
}

glm::dvec3 safeUp(
    const CompiledNavigationVolumeSection& a,
    const CompiledNavigationVolumeSection& b,
    double t,
    const glm::dvec3& forward
) noexcept
{
    glm::dvec3 up = a.upWorld * (1.0 - t) + b.upWorld * t;
    up -= forward * glm::dot(up, forward);
    if (glm::length(up) <= 1.0e-9)
    {
        glm::dvec3 fallback =
            std::abs(forward.y) < 0.9
                ? glm::dvec3(0.0, 1.0, 0.0)
                : glm::dvec3(1.0, 0.0, 0.0);
        up = fallback - forward * glm::dot(fallback, forward);
    }
    return glm::normalize(up);
}

bool pointInsideSpan(
    const CompiledNavigationVolumeSection& a,
    const CompiledNavigationVolumeSection& b,
    const glm::dvec3& point,
    double insetMeters
) noexcept
{
    const glm::dvec3 axis = b.centerWorldMeters - a.centerWorldMeters;
    const double length2 = glm::dot(axis, axis);
    if (length2 <= 1.0e-12)
        return false;

    const double rawT =
        glm::dot(point - a.centerWorldMeters, axis) / length2;
    constexpr double endpointTolerance = 1.0e-9;
    if (rawT < -endpointTolerance || rawT > 1.0 + endpointTolerance)
        return false;

    const double t = std::clamp(rawT, 0.0, 1.0);
    const glm::dvec3 center = a.centerWorldMeters + axis * t;
    const glm::dvec3 forward = glm::normalize(axis);
    const glm::dvec3 up = safeUp(a, b, t, forward);
    const glm::dvec3 right = glm::normalize(glm::cross(forward, up));
    const glm::dvec3 offset = point - center;

    if (a.crossSection != b.crossSection)
        return false;

    if (a.crossSection ==
        world::navigation::NavigationCrossSectionKind::Circle)
    {
        const double radius =
            a.radiusMeters * (1.0 - t) + b.radiusMeters * t -
            insetMeters;
        if (!finitePositive(radius))
            return false;

        const double axial = glm::dot(offset, forward);
        const glm::dvec3 radial = offset - forward * axial;
        return glm::dot(radial, radial) <= radius * radius;
    }

    const double halfWidth =
        a.halfWidthMeters * (1.0 - t) + b.halfWidthMeters * t -
        insetMeters;
    const double halfHeight =
        a.halfHeightMeters * (1.0 - t) + b.halfHeightMeters * t -
        insetMeters;
    if (!finitePositive(halfWidth) || !finitePositive(halfHeight))
        return false;

    return
        std::abs(glm::dot(offset, right)) <= halfWidth &&
        std::abs(glm::dot(offset, up)) <= halfHeight;
}

double estimatedCurveLength(
    const planner::RouteCurveSegment& curve
) noexcept
{
    const double authored =
        curve.endProgressMeters - curve.startProgressMeters;
    if (std::isfinite(authored) && authored > 1.0e-9)
        return authored;

    if (curve.kind == planner::RouteCurveKind::Line)
        return glm::length(curve.endMeters - curve.startMeters);

    if (curve.kind == planner::RouteCurveKind::CircularArc &&
        std::isfinite(curve.arcRadiusMeters) &&
        std::isfinite(curve.arcSweepRadians))
    {
        return
            std::abs(curve.arcRadiusMeters * curve.arcSweepRadians);
    }

    // Conservative numerical estimate for Bezier or malformed fallback.
    double length = 0.0;
    glm::dvec3 previous = curve.positionAtParameter(0.0);
    constexpr int samples = 128;
    for (int i = 1; i <= samples; ++i)
    {
        const double t = static_cast<double>(i) / samples;
        const glm::dvec3 current = curve.positionAtParameter(t);
        length += glm::length(current - previous);
        previous = current;
    }
    return length;
}


bool progressAtPoint(
    const std::vector<planner::RouteCurveSegment>& curves,
    const glm::dvec3& point,
    double& progressMeters
) noexcept
{
    constexpr double toleranceMeters = 1.0e-4;
    double bestDistance = std::numeric_limits<double>::infinity();
    double bestProgress = 0.0;

    for (const auto& curve : curves)
    {
        if (curve.kind == planner::RouteCurveKind::Line)
        {
            const glm::dvec3 delta = curve.endMeters - curve.startMeters;
            const double length2 = glm::dot(delta, delta);
            if (length2 <= 1.0e-12)
                continue;

            const double t = std::clamp(
                glm::dot(point - curve.startMeters, delta) / length2,
                0.0,
                1.0
            );
            const glm::dvec3 candidate =
                curve.startMeters + delta * t;
            const double distance = glm::length(candidate - point);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestProgress =
                    curve.startProgressMeters +
                    (curve.endProgressMeters -
                     curve.startProgressMeters) * t;
            }
            continue;
        }

        // Arc/Bezier boundaries are uncommon for authored portals, but keep
        // the locator generic. Coarse search followed by local ternary
        // refinement is deterministic and works on the canonical curve.
        constexpr int coarseSamples = 256;
        int bestIndex = 0;
        double localBest = std::numeric_limits<double>::infinity();
        for (int i = 0; i <= coarseSamples; ++i)
        {
            const double t =
                static_cast<double>(i) /
                static_cast<double>(coarseSamples);
            const double distance =
                glm::length(curve.positionAtParameter(t) - point);
            if (distance < localBest)
            {
                localBest = distance;
                bestIndex = i;
            }
        }

        double lo = std::max(
            0.0,
            static_cast<double>(bestIndex - 1) / coarseSamples
        );
        double hi = std::min(
            1.0,
            static_cast<double>(bestIndex + 1) / coarseSamples
        );
        for (int iteration = 0; iteration < 32; ++iteration)
        {
            const double a = lo + (hi - lo) / 3.0;
            const double b = hi - (hi - lo) / 3.0;
            const double da =
                glm::length(curve.positionAtParameter(a) - point);
            const double db =
                glm::length(curve.positionAtParameter(b) - point);
            if (da <= db)
                hi = b;
            else
                lo = a;
        }

        const double t = 0.5 * (lo + hi);
        const double distance =
            glm::length(curve.positionAtParameter(t) - point);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestProgress =
                curve.startProgressMeters +
                (curve.endProgressMeters -
                 curve.startProgressMeters) * t;
        }
    }

    if (!std::isfinite(bestDistance) ||
        bestDistance > toleranceMeters)
    {
        return false;
    }

    progressMeters = bestProgress;
    return true;
}

const planner::RouteCurveSegment* curveAtProgress(
    const std::vector<planner::RouteCurveSegment>& curves,
    double progressMeters
) noexcept
{
    for (const auto& curve : curves)
    {
        if (progressMeters + 1.0e-9 >= curve.startProgressMeters &&
            progressMeters - 1.0e-9 <= curve.endProgressMeters)
        {
            return &curve;
        }
    }
    return nullptr;
}

} // namespace

bool RouteVolumeContainmentValidator::pointInsideErodedVolume(
    const CompiledTrafficStage& stage,
    const glm::dvec3& pointWorldMeters,
    double agentRadiusMeters,
    double additionalInsetMeters
) noexcept
{
    if (!finite3(pointWorldMeters) ||
        !std::isfinite(agentRadiusMeters) ||
        agentRadiusMeters < 0.0 ||
        !std::isfinite(additionalInsetMeters) ||
        additionalInsetMeters < 0.0 ||
        stage.volumeSections.size() < 2)
    {
        return false;
    }

    const double inset =
        agentRadiusMeters +
        stage.volumeRequiredClearanceMeters +
        additionalInsetMeters;

    for (std::size_t i = 1; i < stage.volumeSections.size(); ++i)
    {
        if (pointInsideSpan(
                stage.volumeSections[i - 1],
                stage.volumeSections[i],
                pointWorldMeters,
                inset))
        {
            return true;
        }
    }

    return false;
}

RouteVolumeContainmentResult
RouteVolumeContainmentValidator::validateKeepInsideRouteInterval(
    const CompiledTrafficStage& stage,
    const std::vector<planner::RouteCurveSegment>& curves,
    const glm::dvec3& entryWorldMeters,
    const glm::dvec3& exitWorldMeters,
    double agentRadiusMeters
)
{
    RouteVolumeContainmentResult out;

    if (stage.kind != TrafficRouteStageKind::VolumeTransit ||
        stage.volumeConstraint.policy !=
            world::navigation::NavigationVolumePolicy::KeepInside)
    {
        out.failure =
            "route interval containment requires KeepInside volume transit";
        return out;
    }

    if (curves.empty() ||
        !finite3(entryWorldMeters) ||
        !finite3(exitWorldMeters) ||
        !std::isfinite(agentRadiusMeters) ||
        agentRadiusMeters < 0.0)
    {
        out.failure = "invalid route interval containment input";
        return out;
    }

    double entryProgress = 0.0;
    double exitProgress = 0.0;
    if (!progressAtPoint(curves, entryWorldMeters, entryProgress))
    {
        out.failure = "BLUE entry is not on canonical route geometry";
        return out;
    }
    if (!progressAtPoint(curves, exitWorldMeters, exitProgress))
    {
        out.failure = "BLUE exit is not on canonical route geometry";
        return out;
    }
    if (exitProgress <= entryProgress + 1.0e-6)
    {
        out.failure = "BLUE route interval has invalid boundary order";
        return out;
    }

    out.requiredInsetMeters =
        agentRadiusMeters + stage.volumeRequiredClearanceMeters;

    const double length = exitProgress - entryProgress;
    const std::size_t sampleCount = std::clamp<std::size_t>(
        static_cast<std::size_t>(
            std::ceil(length / kMaxSampleSpacingMeters)
        ) + 1,
        2,
        kMaxSamplesPerCurve
    );

    for (std::size_t i = 0; i < sampleCount; ++i)
    {
        const double u =
            static_cast<double>(i) /
            static_cast<double>(sampleCount - 1);
        const double progress =
            entryProgress + length * u;
        const auto* curve = curveAtProgress(curves, progress);
        if (!curve)
        {
            out.failure = "BLUE interval has a gap in canonical route curves";
            return out;
        }

        const double t = curve->parameterAtProgress(progress);
        const glm::dvec3 point = curve->positionAtParameter(t);
        if (!pointInsideErodedVolume(
                stage,
                point,
                agentRadiusMeters,
                kNumericalInsetMeters))
        {
            out.failure =
                "canonical route leaves eroded KeepInside interval";
            out.parameter01 = t;
            out.offendingPointMeters = point;
            return out;
        }
    }

    out.valid = true;
    return out;
}

RouteVolumeContainmentResult
RouteVolumeContainmentValidator::validateKeepInside(
    const CompiledTrafficStage& stage,
    const std::vector<planner::RouteCurveSegment>& curves,
    double agentRadiusMeters
)
{
    RouteVolumeContainmentResult out;

    if (stage.kind != TrafficRouteStageKind::VolumeTransit ||
        stage.volumeConstraint.policy !=
            world::navigation::NavigationVolumePolicy::KeepInside)
    {
        out.failure = "containment validator requires KeepInside volume transit";
        return out;
    }

    if (!std::isfinite(agentRadiusMeters) || agentRadiusMeters < 0.0)
    {
        out.failure = "invalid agent radius for volume containment";
        return out;
    }

    if (stage.volumeSections.size() < 2)
    {
        out.failure = "compiled KeepInside stage has no swept volume geometry";
        return out;
    }

    if (curves.empty())
    {
        out.failure = "KeepInside stage has no route curves";
        return out;
    }

    out.requiredInsetMeters =
        agentRadiusMeters + stage.volumeRequiredClearanceMeters;

    for (std::size_t curveIndex = 0;
         curveIndex < curves.size();
         ++curveIndex)
    {
        const auto& curve = curves[curveIndex];
        const double length = estimatedCurveLength(curve);
        if (!std::isfinite(length) || length < 0.0)
        {
            out.failure = "route curve has invalid length";
            out.curveIndex = curveIndex;
            return out;
        }

        const std::size_t sampleCount = std::clamp<std::size_t>(
            static_cast<std::size_t>(
                std::ceil(length / kMaxSampleSpacingMeters)
            ) + 1,
            2,
            kMaxSamplesPerCurve
        );

        for (std::size_t i = 0; i < sampleCount; ++i)
        {
            const double t =
                sampleCount > 1
                    ? static_cast<double>(i) /
                        static_cast<double>(sampleCount - 1)
                    : 0.0;
            const glm::dvec3 point = curve.positionAtParameter(t);

            // Tiny extra inset keeps numerical equality at an eroded boundary
            // from being accepted as a safe authored route.
            if (!pointInsideErodedVolume(
                    stage,
                    point,
                    agentRadiusMeters,
                    kNumericalInsetMeters))
            {
                out.failure =
                    "route curve leaves eroded KeepInside navigation volume";
                out.curveIndex = curveIndex;
                out.parameter01 = t;
                out.offendingPointMeters = point;
                return out;
            }
        }
    }

    out.valid = true;
    return out;
}

} // namespace game::navigation::traffic
