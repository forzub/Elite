#include "src/game/navigation/autopilot/RouteExecutionCore.h"

namespace game::navigation::autopilot
{

bool RouteExecutionCore::buildContinuousReference(
    const std::vector<AcceptedManeuverProgram>& programs,
    std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
    std::vector<double>& progress
)
{
    samples.clear();
    progress.clear();

    for (std::size_t pageIndex = 0;
         pageIndex < programs.size();
         ++pageIndex)
    {
        const auto& page = programs[pageIndex];
        if (!page.valid || page.sampleCount < 2)
            return false;

        const std::size_t first =
            pageIndex == 0 ? 0 : 1;
        for (std::size_t i = first;
             i < page.sampleCount;
             ++i)
        {
            samples.push_back(page.samples[i]);
        }
    }

    if (samples.size() < 2)
        return false;

    progress.resize(samples.size(), 0.0);
    for (std::size_t i = 1; i < samples.size(); ++i)
    {
        const double ds = glm::length(
            samples[i].positionMapMeters -
            samples[i - 1].positionMapMeters
        );
        if (!std::isfinite(ds) || ds <= 1.0e-9)
            return false;
        progress[i] = progress[i - 1] + ds;
    }
    return true;
}

RouteExecutionCore::ContinuousReferenceDiagnostic
RouteExecutionCore::sampleContinuousReference(
    const std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
    const std::vector<double>& progress,
    const glm::dvec3& positionMapMeters,
    std::size_t minimumSegmentIndex
) noexcept
{
    ContinuousReferenceDiagnostic out;
    if (samples.size() < 2 ||
        progress.size() != samples.size())
    {
        return out;
    }

    std::size_t segment = std::min(
        minimumSegmentIndex,
        samples.size() - 2
    );

    // Advance strictly along the already accepted centerline. Crossing
    // the plane beyond a sample advances to the next segment; nearest
    // global-point search is deliberately avoided so a route that passes
    // near itself cannot jump to a later branch.
    for (;;)
    {
        const glm::dvec3 a =
            samples[segment].positionMapMeters;
        const glm::dvec3 b =
            samples[segment + 1].positionMapMeters;
        const glm::dvec3 delta = b - a;
        const double length2 = glm::dot(delta, delta);
        if (!(std::isfinite(length2) && length2 > 1.0e-18))
            return out;

        const double raw =
            glm::dot(positionMapMeters - a, delta) / length2;

        if (raw > 1.0 &&
            segment + 2 < samples.size())
        {
            ++segment;
            continue;
        }

        const double u = std::clamp(raw, 0.0, 1.0);
        const auto& lower = samples[segment];
        const auto& upper = samples[segment + 1];

        out.valid = true;
        out.lowerSampleIndex = segment;
        out.upperSampleIndex = segment + 1;
        out.interpolation01 = u;
        out.spatialProgressMeters =
            progress[segment] +
            (progress[segment + 1] - progress[segment]) * u;

        auto& ref = out.reference;
        ref.timeOffsetSeconds =
            lower.timeOffsetSeconds * (1.0 - u) +
            upper.timeOffsetSeconds * u;
        ref.positionMapMeters =
            glm::mix(
                lower.positionMapMeters,
                upper.positionMapMeters,
                u
            );
        ref.velocityMapMetersPerSecond =
            glm::mix(
                lower.velocityMapMetersPerSecond,
                upper.velocityMapMetersPerSecond,
                u
            );
        ref.linearAccelerationFeedForwardMapMps2 =
            glm::mix(
                lower.linearAccelerationFeedForwardMapMps2,
                upper.linearAccelerationFeedForwardMapMps2,
                u
            );

        ref.forwardMap = normalizedOr(
            glm::mix(lower.forwardMap, upper.forwardMap, u),
            lower.forwardMap
        );

        glm::dvec3 up = glm::mix(
            lower.upMap,
            upper.upMap,
            u
        );
        up -= ref.forwardMap * glm::dot(up, ref.forwardMap);
        ref.upMap = normalizedOr(up, lower.upMap);
        ref.rightMap = normalizedOr(
            glm::cross(ref.forwardMap, ref.upMap),
            lower.rightMap
        );
        ref.upMap = normalizedOr(
            glm::cross(ref.rightMap, ref.forwardMap),
            ref.upMap
        );

        ref.angularVelocityMapRadPerSecond =
            glm::mix(
                lower.angularVelocityMapRadPerSecond,
                upper.angularVelocityMapRadPerSecond,
                u
            );
        ref.angularAccelerationFeedForwardMapRadPerSec2 =
            glm::mix(
                lower.angularAccelerationFeedForwardMapRadPerSec2,
                upper.angularAccelerationFeedForwardMapRadPerSec2,
                u
            );
        return out;
    }
}

bool RouteExecutionCore::initializeRuntimeProfile(
    const std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
    std::vector<double>& nominalSpeeds,
    std::vector<double>& runtimeSpeeds,
    std::vector<double>& runtimeAccelerations
)
{
    if (samples.size() < 2)
        return false;

    nominalSpeeds.resize(samples.size(), 0.0);
    runtimeSpeeds.resize(samples.size(), 0.0);
    runtimeAccelerations.resize(samples.size(), 0.0);

    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        const double speed =
            glm::length(samples[i].velocityMapMetersPerSecond);
        if (!std::isfinite(speed) || speed < 0.0)
            return false;
        nominalSpeeds[i] = speed;
        runtimeSpeeds[i] = speed;
    }

    // A speed sample is a boundary condition. Derive the acceleration
    // needed over each non-zero spatial interval from v^2 = v0^2 + 2*a*ds
    // instead of assuming that a zero-speed checkpoint means a=0.
    for (std::size_t i = 0; i + 1 < samples.size(); ++i)
    {
        const double ds = glm::length(
            samples[i + 1].positionMapMeters -
            samples[i].positionMapMeters
        );
        if (!(std::isfinite(ds) && ds > 1.0e-9))
            return false;

        runtimeAccelerations[i] =
            (runtimeSpeeds[i + 1] * runtimeSpeeds[i + 1] -
             runtimeSpeeds[i] * runtimeSpeeds[i]) /
            (2.0 * ds);
    }
    runtimeAccelerations.back() =
        runtimeAccelerations[runtimeAccelerations.size() - 2];

    return true;
}

bool RouteExecutionCore::buildCheckpointProgress(
    const std::vector<planner::RouteGate>& gates,
    const std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
    const std::vector<double>& progress,
    std::vector<double>& checkpoints
)
{
    checkpoints.clear();
    if (gates.empty() ||
        samples.size() < 2 ||
        progress.size() != samples.size())
    {
        return false;
    }

    std::size_t minimumSegment = 0;
    for (const auto& gate : gates)
    {
        double bestDistance2 =
            std::numeric_limits<double>::infinity();
        double bestProgress = progress[minimumSegment];
        std::size_t bestSegment = minimumSegment;

        for (std::size_t segment = minimumSegment;
             segment + 1 < samples.size();
             ++segment)
        {
            const glm::dvec3 a =
                samples[segment].positionMapMeters;
            const glm::dvec3 b =
                samples[segment + 1].positionMapMeters;
            const glm::dvec3 delta = b - a;
            const double length2 = glm::dot(delta, delta);
            if (!(std::isfinite(length2) && length2 > 1.0e-18))
                continue;

            const double u = std::clamp(
                glm::dot(gate.positionMeters - a, delta) / length2,
                0.0,
                1.0
            );
            const glm::dvec3 projected = a + delta * u;
            const glm::dvec3 error =
                gate.positionMeters - projected;
            const double distance2 = glm::dot(error, error);

            if (distance2 < bestDistance2)
            {
                bestDistance2 = distance2;
                bestSegment = segment;
                bestProgress =
                    progress[segment] +
                    (progress[segment + 1] - progress[segment]) * u;
            }
        }

        if (!std::isfinite(bestDistance2) ||
            !std::isfinite(bestProgress))
        {
            return false;
        }

        if (!checkpoints.empty())
            bestProgress = std::max(checkpoints.back(), bestProgress);

        checkpoints.push_back(bestProgress);
        minimumSegment = bestSegment;
    }

    return !checkpoints.empty();
}

double RouteExecutionCore::referenceNormalAccelerationMps2(
    const AcceptedManeuverProgram::ReferenceSample& sample,
    double& speedMps) noexcept
{
    speedMps = glm::length(sample.velocityMapMetersPerSecond);
    if (!(speedMps > 1.0e-3))
        return 0.0;
    const glm::dvec3 t = sample.velocityMapMetersPerSecond / speedMps;
    const glm::dvec3 a = sample.linearAccelerationFeedForwardMapMps2;
    return glm::length(a - t * glm::dot(a, t));
}

void RouteExecutionCore::limitNarrowSpeedsToRcs(
    State& state, double progressMeters, double share) noexcept
{
    share = std::clamp(share, 0.0, 1.0);
    for (const auto& zone : state.narrowZones)
    {
        if (zone.endProgressMeters <= progressMeters)
            continue;
        // The approach plateau lasts NarrowApproachSeconds at the zone
        // speed (the speed at the zone entry).
        double zoneSpeed = 0.0;
        for (std::size_t i = 0; i < state.nominalSpeedProfileMps.size(); ++i)
            if (state.continuousProgressMeters[i] >= zone.startProgressMeters)
            {
                zoneSpeed = state.nominalSpeedProfileMps[i];
                break;
            }
        const double approach =
            zoneSpeed * NewtonianFlightPolicy::NarrowApproachSeconds;
        for (std::size_t i = state.currentContinuousSegment;
             i < state.nominalSpeedProfileMps.size(); ++i)
        {
            const double s = state.continuousProgressMeters[i];
            if (s >= zone.startProgressMeters - approach &&
                s <= zone.endProgressMeters)
                state.nominalSpeedProfileMps[i] *= share;
        }
    }
}

void RouteExecutionCore::limitCurveSpeedsToLateral(State& state, double lateralMps2) noexcept
{
    if (!(lateralMps2 > 1.0e-9))
        return;
    for (std::size_t i = state.currentContinuousSegment;
         i < state.nominalSpeedProfileMps.size() &&
         i < state.continuousSamples.size(); ++i)
    {
        double v = 0.0;
        const double normal =
            referenceNormalAccelerationMps2(state.continuousSamples[i], v);
        if (normal > lateralMps2)
            state.nominalSpeedProfileMps[i] = std::min(
                state.nominalSpeedProfileMps[i],
                v * std::sqrt(lateralMps2 / normal));
    }
}

bool RouteExecutionCore::canBrakeToSpeedsAhead(
    const State& state,
    double progressMeters,
    double speedMps,
    double brakingMps2,
    double leadSeconds,
    double fullLateralMps2
) noexcept
{
    if (!(brakingMps2 > 1.0e-9))
        return false;
    const double lost = speedMps * std::max(0.0, leadSeconds);
    for (std::size_t i = state.currentContinuousSegment + 1;
         i < state.nominalSpeedProfileMps.size() &&
         i < state.continuousSamples.size(); ++i)
    {
        double limit = state.nominalSpeedProfileMps[i];
        double v = 0.0;
        const double normal =
            referenceNormalAccelerationMps2(state.continuousSamples[i], v);
        if (normal > 1.0e-6)
            limit = std::max(limit,
                v * std::sqrt(std::max(0.0, fullLateralMps2) / normal));
        // Inside the speed tolerance band the craft is on its profile.
        limit += NewtonianSpeedTolerancePolicy::softBandMps(limit);
        if (speedMps <= limit)
            continue;
        const double room =
            state.continuousProgressMeters[i] - progressMeters - lost;
        if (room <= 0.0 ||
            speedMps * speedMps - 2.0 * brakingMps2 * room >
                limit * limit + 1.0)
            return false;
    }
    return true;
}

void RouteExecutionCore::recomputeRuntimeSpeedSuffix(
    const std::vector<double>& nominalSpeeds,
    const std::vector<double>& progress,
    std::size_t anchorIndex,
    double actualSpeedMps,
    double acceleratingMps2,
    double brakingMps2,
    std::vector<double>& runtimeSpeeds,
    std::vector<double>& runtimeAccelerations,
    double thrustReversalCoastSeconds,
    std::size_t toleratedPlateauEndIndex
) noexcept
{
    if (nominalSpeeds.size() < 2 ||
        progress.size() != nominalSpeeds.size() ||
        runtimeSpeeds.size() != nominalSpeeds.size() ||
        runtimeAccelerations.size() != nominalSpeeds.size())
    {
        return;
    }

    anchorIndex = std::min(anchorIndex, nominalSpeeds.size() - 1);

    for (std::size_t i = anchorIndex;
         i < nominalSpeeds.size();
         ++i)
    {
        runtimeSpeeds[i] = nominalSpeeds[i];
    }
    // Speed tolerance corridor: the plateau ahead is re-levelled at the
    // measured speed (inside the band; see toleratedPlateauEnd).
    if (std::isfinite(actualSpeedMps))
        for (std::size_t i = anchorIndex;
             i < std::min(toleratedPlateauEndIndex, nominalSpeeds.size());
             ++i)
            runtimeSpeeds[i] = std::max(0.0, actualSpeedMps);

    // Forward reachability from the measured checkpoint speed. If the
    // craft arrived slower than nominal, later targets are lowered until
    // main-engine acceleration can physically catch the nominal profile.
    double reachableFromActual =
        std::max(0.0, std::isfinite(actualSpeedMps) ? actualSpeedMps : 0.0);
    for (std::size_t i = anchorIndex + 1;
         i < runtimeSpeeds.size();
         ++i)
    {
        const double ds = progress[i] - progress[i - 1];
        if (!(std::isfinite(ds) && ds >= 0.0))
            return;

        if (acceleratingMps2 > 1.0e-9)
        {
            reachableFromActual =
                std::sqrt(
                    std::max(
                        0.0,
                        reachableFromActual * reachableFromActual +
                        2.0 * acceleratingMps2 * ds
                    )
                );
            runtimeSpeeds[i] =
                std::min(runtimeSpeeds[i], reachableFromActual);
        }
    }

    // Backward braking feasibility preserves every future nominal speed
    // restriction and propagates it toward the checkpoint.
    if (brakingMps2 > 1.0e-9)
    {
        for (std::size_t i = runtimeSpeeds.size() - 1;
             i > anchorIndex;
             --i)
        {
            const double ds = progress[i] - progress[i - 1];
            if (!(std::isfinite(ds) && ds >= 0.0))
                return;

            const double reachable =
                std::sqrt(
                    std::max(
                        0.0,
                        runtimeSpeeds[i] * runtimeSpeeds[i] +
                        2.0 * brakingMps2 * ds
                    )
                );
            runtimeSpeeds[i - 1] =
                std::min(runtimeSpeeds[i - 1], reachable);
        }
    }

    if (thrustReversalCoastSeconds > 0.0)
        insertThrustReversalCoast(
            progress,
            anchorIndex,
            std::isfinite(actualSpeedMps)
                ? std::max(0.0, actualSpeedMps) : 0.0,
            acceleratingMps2,
            thrustReversalCoastSeconds,
            runtimeSpeeds
        );
    // The measured checkpoint speed is authoritative. If it is above
    // the speed from which a future route limit is reachable, do not
    // invalidate the route: propagate the fastest physically possible
    // braking profile forward until the authored limits are caught again.
    double physicalSpeed =
        std::max(
            0.0,
            std::isfinite(actualSpeedMps) ? actualSpeedMps : 0.0
        );
    runtimeSpeeds[anchorIndex] = physicalSpeed;
    if (brakingMps2 > 1.0e-9)
    {
        for (std::size_t i = anchorIndex + 1;
             i < runtimeSpeeds.size();
             ++i)
        {
            const double ds = progress[i] - progress[i - 1];
            if (!(std::isfinite(ds) && ds >= 0.0))
                return;

            const double minimumReachable =
                std::sqrt(
                    std::max(
                        0.0,
                        physicalSpeed * physicalSpeed -
                        2.0 * brakingMps2 * ds
                    )
                );
            runtimeSpeeds[i] =
                std::max(runtimeSpeeds[i], minimumReachable);
            physicalSpeed = runtimeSpeeds[i];
        }
    }

    if (!nominalSpeeds.empty() &&
        nominalSpeeds.back() <= 1.0e-9)
    {
        runtimeSpeeds.back() = 0.0;
    }

    // Reconstruct longitudinal feed-forward from v^2 relation. Geometry
    // and lateral/angular references remain exactly the accepted program.
    for (std::size_t i = anchorIndex;
         i < runtimeAccelerations.size();
         ++i)
    {
        runtimeAccelerations[i] = 0.0;
    }
    for (std::size_t i = anchorIndex;
         i + 1 < runtimeSpeeds.size();
         ++i)
    {
        const double ds = progress[i + 1] - progress[i];
        if (ds > 1.0e-9)
        {
            runtimeAccelerations[i] =
                (runtimeSpeeds[i + 1] * runtimeSpeeds[i + 1] -
                 runtimeSpeeds[i] * runtimeSpeeds[i]) /
                (2.0 * ds);
        }
    }

}

std::FILE* RouteExecutionCore::profileTraceFile() noexcept
{
    static std::FILE* file =
        []() -> std::FILE*
        {
            const char* path =
                std::getenv("ELITE_NEWTONIAN_PROFILE_TRACE");
            return path && *path ? std::fopen(path, "w") : nullptr;
        }();
    return file;
}

void RouteExecutionCore::dumpAcceptedRoute(
    const State& state,
    const planner::RoutePlan& plan
) noexcept
{
    static std::FILE* file =
        []() -> std::FILE*
        {
            const char* path = std::getenv("ELITE_ROUTE_DUMP");
            return path && *path ? std::fopen(path, "w") : nullptr;
        }();
    if (!file)
        return;
    std::fprintf(file, "ROUTE curves=%zu exec_gates=%zu samples=%zu\n",
        plan.routeCurves.size(), plan.executionGates.size(),
        state.continuousSamples.size());
    for (const auto& c : plan.routeCurves)
        std::fprintf(file, "C kind=%d s0=%.1f len=%.1f radius=%.1f "
            "vmax=%.2f\n", static_cast<int>(c.kind),
            c.startProgressMeters,
            c.endProgressMeters - c.startProgressMeters,
            c.arcRadiusMeters, c.maxSpeedMps);
    const auto& speeds = state.runtimeSpeedProfileMps;
    for (std::size_t i = 1; i + 1 < speeds.size(); ++i)
        if (speeds[i] <= 0.5 && speeds[i] <= speeds[i - 1] &&
            speeds[i] < speeds[i + 1])
            std::fprintf(file, "STOP s=%.1f v=%.2f\n",
                state.continuousProgressMeters[i], speeds[i]);
    double nextS = 0.0;
    for (std::size_t i = 0; i < state.runtimeSpeedProfileMps.size(); ++i)
        if (state.continuousProgressMeters[i] >= nextS ||
            i + 1 == state.runtimeSpeedProfileMps.size())
        {
            std::fprintf(file, "V %.1f %.2f\n",
                state.continuousProgressMeters[i],
                state.runtimeSpeedProfileMps[i]);
            nextS = state.continuousProgressMeters[i] + 50.0;
        }
    std::fflush(file);
}

void RouteExecutionCore::traceCapabilityChange(
    const State& state,
    double universeTimeSeconds,
    int kind,
    double actualSpeedMps,
    const NavigationControlCapability& live,
    const char* action
) noexcept
{
    const auto& before = state.capabilityWatch.accepted;
    std::fprintf(stderr,
        "[CapabilityChange] t=%.2f kind=%d speed=%.1f main=%.2f->%.2f "
        "reverse=%.2f->%.2f rcs=%.3f->%.3f angular=%.4f->%.4f action=%s\n",
        universeTimeSeconds, kind, actualSpeedMps,
        before.mainForwardMps2, live.mainForwardMps2,
        before.mainReverseMps2, live.mainReverseMps2,
        before.rcsScalarMps2, live.rcsScalarMps2,
        before.angularScalarRadPerSec2, live.angularScalarRadPerSec2,
        action);
}

void RouteExecutionCore::traceProfile(
    const State& state,
    double universeTimeSeconds,
    double actualSpeedMps
) noexcept
{
    std::FILE* file = profileTraceFile();
    if (!file)
        return;
    std::fprintf(file, "R t=%.3f rev=%llu anchor=%zu actual=%.2f\n",
        universeTimeSeconds,
        static_cast<unsigned long long>(state.speedProfileRevision),
        state.currentContinuousSegment, actualSpeedMps);
    for (std::size_t i = state.currentContinuousSegment;
         i < state.runtimeSpeedProfileMps.size(); ++i)
        std::fprintf(file, "P %zu %.1f %.2f %.2f\n", i,
            state.continuousProgressMeters[i],
            state.nominalSpeedProfileMps[i],
            state.runtimeSpeedProfileMps[i]);
    std::fflush(file);
}

void RouteExecutionCore::insertThrustReversalCoast(
    const std::vector<double>& progress,
    std::size_t anchorIndex,
    double anchorSpeedMps,
    double acceleratingMps2,
    double coastSeconds,
    std::vector<double>& speeds
) noexcept
{
    const std::size_t count = speeds.size();
    if (count < 2 || progress.size() != count ||
        anchorIndex + 1 >= count ||
        !(std::isfinite(coastSeconds) && coastSeconds > 0.0))
    {
        return;
    }

    const std::vector<double> braking = speeds;

    // The coast must stay under the braking profile over its WHOLE
    // length, not only at its end (a coast must never jump over a low
    // point such as a corner). Sparse-table range minimum of samples.
    std::vector<std::vector<double>> minimumTable {braking};
    for (std::size_t width = 1; 2 * width <= count; width *= 2)
    {
        const auto& previous = minimumTable.back();
        std::vector<double> level(count - 2 * width + 1);
        for (std::size_t i = 0; i < level.size(); ++i)
            level[i] = std::min(previous[i], previous[i + width]);
        minimumTable.push_back(std::move(level));
    }
    std::vector<unsigned char> floorLog2(count + 1, 0);
    for (std::size_t n = 2; n <= count; ++n)
        floorLog2[n] = static_cast<unsigned char>(floorLog2[n / 2] + 1);

    /*
        Minimum of the braking profile over a coast window starting at
        sample i and ending at progress s (s >= progress[i]): the
        interpolated speed at s and every sample in between.

        This runs 24 x per remaining sample at every checkpoint, so it
        was the frame hitch of route flights (~75 ms per checkpoint in
        the Debug build). It reproduces the former
        upper_bound-over-the-whole-route computation exactly: every
        sample up to i is <= s, so the first sample above s lies after i
        and is found by galloping from i (the window is short against
        the route); the same index serves the interpolation and the
        range minimum; the arithmetic is unchanged (bit-identical).
    */
    const double lastProgress = progress.back();
    const double lastBraking = braking.back();
    const auto brakingMinimumOver =
        [&](std::size_t from, double toProgress) noexcept
        {
            // upper = first index with progress[upper] > toProgress.
            std::size_t low = from;          // progress[low] <= toProgress
            std::size_t high = from + 1;
            std::size_t step = 1;
            while (high < count && progress[high] <= toProgress)
            {
                low = high;
                high = from + 1 + step;
                step *= 2;
            }
            if (high > count)
                high = count;
            while (low + 1 < high)
            {
                const std::size_t mid = low + (high - low) / 2;
                if (progress[mid] <= toProgress)
                    low = mid;
                else
                    high = mid;
            }
            const std::size_t upper = high;

            double minimum;
            if (toProgress >= lastProgress)
            {
                minimum = lastBraking;
            }
            else
            {
                const std::size_t i = std::max<std::size_t>(1, upper);
                const double span = progress[i] - progress[i - 1];
                const double u = span > 1.0e-9
                    ? std::clamp((toProgress - progress[i - 1]) / span, 0.0, 1.0)
                    : 1.0;
                // v^2 is linear in s under constant acceleration.
                minimum = std::sqrt(std::max(0.0,
                    braking[i - 1] * braking[i - 1] * (1.0 - u) +
                    braking[i] * braking[i] * u));
            }
            if (upper > from)
            {
                const std::size_t first = from;
                const std::size_t last = upper - 1;
                const std::size_t level = floorLog2[last - first + 1];
                minimum = std::min(minimum, std::min(
                    minimumTable[level][first],
                    minimumTable[level][last + 1 - (std::size_t(1) << level)]
                ));
            }
            return minimum;
        };

    /*
        coastReady[i] is a pure function of progress, coastSeconds and
        the braking profile from sample i to the end of the route. A
        checkpoint usually re-levels only the profile near the craft, so
        the samples whose whole braking suffix is unchanged since the last
        call reuse their previous value exactly (bit-identical); only the
        changed head is bisected again. Per thread: the client autopilot
        and offline workers never share a cache.
    */
    struct CoastCache
    {
        std::vector<double> progress;
        std::vector<double> braking;
        std::vector<double> coastReady;
        double coastSeconds = -1.0;
        std::size_t firstValid = 0;
    };
    thread_local CoastCache cache;
    std::size_t reuseFrom = count;
    if (cache.coastSeconds == coastSeconds &&
        cache.progress.size() == count &&
        cache.progress == progress)
    {
        reuseFrom = count;
        while (reuseFrom > 0 && cache.braking[reuseFrom - 1] == braking[reuseFrom - 1])
            --reuseFrom;
        reuseFrom = std::max(reuseFrom, cache.firstValid);
    }
    std::vector<double> coastReady(count, 0.0);
    for (std::size_t i = std::max(reuseFrom, anchorIndex); i < count; ++i)
        coastReady[i] = cache.coastReady[i];
    for (std::size_t i = anchorIndex; i < std::min(reuseFrom, count); ++i)
    {
        double low = 0.0;
        double high = braking[i];
        if (high <=
            brakingMinimumOver(i, progress[i] + high * coastSeconds))
        {
            coastReady[i] = high;
            continue;
        }
        for (int iteration = 0; iteration < 24; ++iteration)
        {
            const double mid = 0.5 * (low + high);
            if (mid <=
                brakingMinimumOver(i, progress[i] + mid * coastSeconds))
                low = mid;
            else
                high = mid;
        }
        coastReady[i] = low;
    }
    cache.progress = progress;
    cache.braking = braking;
    cache.coastReady = coastReady;
    cache.coastSeconds = coastSeconds;
    cache.firstValid = anchorIndex;

    // Start from the measured speed: the anchor sample still holds the
    // nominal value until the physical-floor pass below replaces it.
    double speed = std::min(speeds[anchorIndex], anchorSpeedMps);
    for (std::size_t i = anchorIndex + 1; i < count; ++i)
    {
        const double ds = progress[i] - progress[i - 1];
        if (!(std::isfinite(ds) && ds >= 0.0))
            return;
        // Accelerate only up to the speed from which this sample can
        // still start its coast; at or above it, hold the speed.
        double next = speed;
        if (speed < coastReady[i])
        {
            const double accelerated =
                acceleratingMps2 > 1.0e-9
                    ? std::sqrt(speed * speed +
                          2.0 * acceleratingMps2 * ds)
                    : braking[i];
            next = std::min(accelerated, coastReady[i]);
        }
        speed = std::min(braking[i], next);
        speeds[i] = speed;
    }
}

void RouteExecutionCore::applyRuntimeProfile(
    const std::vector<double>& runtimeSpeeds,
    const std::vector<double>& runtimeAccelerations,
    LocalFlightControlLaw law,
    ContinuousReferenceDiagnostic& reference
) noexcept
{
    if (!reference.valid ||
        reference.lowerSampleIndex >= runtimeSpeeds.size() ||
        reference.upperSampleIndex >= runtimeSpeeds.size() ||
        runtimeAccelerations.size() != runtimeSpeeds.size())
    {
        return;
    }

    const double u = reference.interpolation01;
    const double speed =
        runtimeSpeeds[reference.lowerSampleIndex] * (1.0 - u) +
        runtimeSpeeds[reference.upperSampleIndex] * u;

    const glm::dvec3 tangent =
        normalizedOr(
            reference.reference.velocityMapMetersPerSecond,
            reference.reference.forwardMap
        );
    reference.reference.velocityMapMetersPerSecond =
        tangent * std::max(0.0, speed);

    const double longitudinalAcceleration =
        runtimeAccelerations[reference.lowerSampleIndex] * (1.0 - u) +
        runtimeAccelerations[reference.upperSampleIndex] * u;

    if (law == LocalFlightControlLaw::Newtonian)
    {
        // Runtime speed replanning owns ONLY the longitudinal component.
        // Newtonian depends on the authored path-normal acceleration to
        // lead a bend with the aft-main vector. Erasing it turns a
        // curvature-following problem into a late cross-track recovery.
        const glm::dvec3 authoredAcceleration =
            reference.reference.linearAccelerationFeedForwardMapMps2;
        const glm::dvec3 authoredLateral =
            authoredAcceleration -
            tangent * glm::dot(authoredAcceleration, tangent);

        reference.reference.linearAccelerationFeedForwardMapMps2 =
            authoredLateral +
            tangent * longitudinalAcceleration;
    }
    else
    {
        // Preserve the currently proven Assisted behavior exactly.
        reference.reference.linearAccelerationFeedForwardMapMps2 =
            tangent * longitudinalAcceleration;
    }
}

glm::dvec3 RouteExecutionCore::rotateAroundAxis(
    const glm::dvec3& value,
    const glm::dvec3& axis,
    double angleRad
) noexcept
{
    const glm::dvec3 n =
        normalizedOr(axis, glm::dvec3(0.0, 1.0, 0.0));
    const double c = std::cos(angleRad);
    const double si = std::sin(angleRad);
    return normalizedOr(
        value * c +
        glm::cross(n, value) * si +
        n * glm::dot(n, value) * (1.0 - c),
        value
    );
}

bool RouteExecutionCore::buildCurveSampleProgressMap(
    const std::vector<planner::RouteCurveSegment>& curves,
    const std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
    const std::vector<double>& progress,
    std::vector<double>& starts,
    std::vector<double>& ends
)
{
    starts.clear();
    ends.clear();

    if (curves.empty())
        return true;
    if (samples.size() < 2 || progress.size() != samples.size())
        return false;

    starts.reserve(curves.size());
    ends.reserve(curves.size());

    std::size_t searchFrom = 0;
    for (const auto& curve : curves)
    {
        auto nearestProgress =
            [&](const glm::dvec3& point, std::size_t from)
            {
                std::size_t best = from;
                double bestDistance =
                    std::numeric_limits<double>::infinity();
                for (std::size_t i = from; i < samples.size(); ++i)
                {
                    const glm::dvec3 delta =
                        samples[i].positionMapMeters - point;
                    const double d = glm::dot(delta, delta);
                    if (d < bestDistance)
                    {
                        bestDistance = d;
                        best = i;
                    }
                    if (i > best + 8 && d > bestDistance * 4.0)
                        break;
                }
                return best;
            };

        const std::size_t startIndex =
            nearestProgress(curve.startMeters, searchFrom);
        const std::size_t endIndex =
            nearestProgress(
                curve.endMeters,
                std::min(startIndex + 1, samples.size() - 1)
            );

        if (endIndex <= startIndex)
            return false;

        starts.push_back(progress[startIndex]);
        ends.push_back(progress[endIndex]);
        searchFrom = endIndex;
    }

    return starts.size() == curves.size() &&
        ends.size() == curves.size();
}

double RouteExecutionCore::mapSampleProgressToRouteProgress(
    const std::vector<planner::RouteCurveSegment>& curves,
    const std::vector<double>& starts,
    const std::vector<double>& ends,
    double sampleProgressMeters
) noexcept
{
    if (curves.empty() ||
        starts.size() != curves.size() ||
        ends.size() != curves.size())
    {
        return std::max(0.0, sampleProgressMeters);
    }

    std::size_t index = curves.size() - 1;
    for (std::size_t i = 0; i < curves.size(); ++i)
    {
        if (sampleProgressMeters <= ends[i] + 1.0e-9)
        {
            index = i;
            break;
        }
    }

    const double ds = ends[index] - starts[index];
    const double u =
        ds > 1.0e-12
            ? std::clamp(
                (sampleProgressMeters - starts[index]) / ds,
                0.0,
                1.0
              )
            : 0.0;
    return
        curves[index].startProgressMeters * (1.0 - u) +
        curves[index].endProgressMeters * u;
}

double RouteExecutionCore::mapRouteProgressToSampleProgress(
    const std::vector<planner::RouteCurveSegment>& curves,
    const std::vector<double>& starts,
    const std::vector<double>& ends,
    double routeProgressMeters
) noexcept
{
    if (curves.empty() ||
        starts.size() != curves.size() ||
        ends.size() != curves.size())
    {
        return std::max(0.0, routeProgressMeters);
    }

    std::size_t index = curves.size() - 1;
    for (std::size_t i = 0; i < curves.size(); ++i)
    {
        if (routeProgressMeters <=
            curves[i].endProgressMeters + 1.0e-9)
        {
            index = i;
            break;
        }
    }

    const double dr =
        curves[index].endProgressMeters -
        curves[index].startProgressMeters;
    const double u =
        dr > 1.0e-12
            ? std::clamp(
                (routeProgressMeters -
                 curves[index].startProgressMeters) / dr,
                0.0,
                1.0
              )
            : 0.0;
    return starts[index] * (1.0 - u) + ends[index] * u;
}

std::vector<planner::RouteCurveSegment>
RouteExecutionCore::buildFallbackRouteCurves(
    const std::vector<planner::RouteGate>& gates
)
{
    std::vector<planner::RouteCurveSegment> curves;
    if (gates.size() < 2)
        return curves;

    double progress = 0.0;
    curves.reserve(gates.size() - 1);
    for (std::size_t i = 1; i < gates.size(); ++i)
    {
        const glm::dvec3 delta =
            gates[i].positionMeters -
            gates[i - 1].positionMeters;
        const double length = glm::length(delta);
        if (length <= 1.0e-9)
            continue;

        planner::RouteCurveSegment curve;
        curve.kind = planner::RouteCurveKind::Line;
        curve.startProgressMeters = progress;
        curve.endProgressMeters = progress + length;
        // Compatibility-only fallback has no independent geometric speed
        // ceiling. The accepted trajectory remains authoritative for
        // speed until Planner publishes real routeCurves.
        curve.maxSpeedMps = 0.0;
        curve.startMeters = gates[i - 1].positionMeters;
        curve.endMeters = gates[i].positionMeters;
        curve.startForward = delta / length;
        curve.endForward = curve.startForward;
        curves.push_back(curve);
        progress += length;
    }
    return curves;
}

RouteExecutionCore::RouteCurveDiagnostic RouteExecutionCore::sampleRouteCurveAtProgress(
    const std::vector<planner::RouteCurveSegment>& curves,
    double routeProgressMeters
) noexcept
{
    RouteCurveDiagnostic out;
    if (curves.empty() || !std::isfinite(routeProgressMeters))
        return out;

    std::size_t index = curves.size() - 1;
    for (std::size_t i = 0; i < curves.size(); ++i)
    {
        if (routeProgressMeters <=
            curves[i].endProgressMeters + 1.0e-9)
        {
            index = i;
            break;
        }
    }

    const auto& curve = curves[index];
    const double parameter =
        curve.parameterAtProgress(routeProgressMeters);
    out.valid = true;
    out.curveIndex = index;
    out.positionMapMeters =
        curve.positionAtParameter(parameter);
    out.tangentMap =
        curve.tangentAtProgress(routeProgressMeters);
    out.curvaturePerMeter =
        curve.curvatureAtProgress(routeProgressMeters);

    if (curve.kind == planner::RouteCurveKind::CircularArc)
    {
        const double normalLength = glm::length(curve.arcNormal);
        if (normalLength > 1.0e-12 &&
            std::abs(curve.arcSweepRadians) > 1.0e-12)
        {
            out.turnNormalMap =
                curve.arcNormal / normalLength *
                (curve.arcSweepRadians >= 0.0 ? 1.0 : -1.0);
        }
    }
    else if (curve.kind == planner::RouteCurveKind::CubicBezier)
    {
        const glm::dvec3 d1 =
            curve.derivativeAtParameter(parameter);
        const glm::dvec3 d2 =
            curve.secondDerivativeAtParameter(parameter);
        const glm::dvec3 cross = glm::cross(d1, d2);
        const double crossLength = glm::length(cross);
        if (crossLength > 1.0e-12)
            out.turnNormalMap = cross / crossLength;
    }

    out.maxSpeedMps = curve.maxSpeedMps;
    return out;
}

RouteExecutionCore::ContinuousReferenceDiagnostic
RouteExecutionCore::sampleContinuousReferenceAtProgress(
    const std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
    const std::vector<double>& progress,
    double spatialProgressMeters
) noexcept
{
    ContinuousReferenceDiagnostic out;
    if (samples.size() < 2 ||
        progress.size() != samples.size() ||
        !std::isfinite(spatialProgressMeters))
    {
        return out;
    }

    const double clamped =
        std::clamp(
            spatialProgressMeters,
            progress.front(),
            progress.back()
        );
    auto upperIt =
        std::upper_bound(
            progress.begin(),
            progress.end(),
            clamped
        );
    std::size_t upper =
        upperIt == progress.end()
            ? progress.size() - 1
            : static_cast<std::size_t>(
                std::distance(progress.begin(), upperIt)
              );
    if (upper == 0)
        upper = 1;
    const std::size_t lower = upper - 1;

    const double ds = progress[upper] - progress[lower];
    const double u =
        ds > 1.0e-12
            ? std::clamp(
                (clamped - progress[lower]) / ds,
                0.0,
                1.0
              )
            : 0.0;

    out.valid = true;
    out.lowerSampleIndex = lower;
    out.upperSampleIndex = upper;
    out.interpolation01 = u;
    out.spatialProgressMeters = clamped;

    const auto& a = samples[lower];
    const auto& b = samples[upper];
    auto& ref = out.reference;
    ref.timeOffsetSeconds =
        a.timeOffsetSeconds * (1.0 - u) +
        b.timeOffsetSeconds * u;
    ref.positionMapMeters =
        glm::mix(a.positionMapMeters, b.positionMapMeters, u);
    ref.velocityMapMetersPerSecond =
        glm::mix(
            a.velocityMapMetersPerSecond,
            b.velocityMapMetersPerSecond,
            u
        );
    ref.linearAccelerationFeedForwardMapMps2 =
        glm::mix(
            a.linearAccelerationFeedForwardMapMps2,
            b.linearAccelerationFeedForwardMapMps2,
            u
        );
    ref.forwardMap =
        normalizedOr(
            glm::mix(a.forwardMap, b.forwardMap, u),
            a.forwardMap
        );
    glm::dvec3 up =
        glm::mix(a.upMap, b.upMap, u);
    up -= ref.forwardMap * glm::dot(up, ref.forwardMap);
    ref.upMap = normalizedOr(up, a.upMap);
    ref.rightMap =
        normalizedOr(
            glm::cross(ref.forwardMap, ref.upMap),
            a.rightMap
        );
    ref.upMap =
        normalizedOr(
            glm::cross(ref.rightMap, ref.forwardMap),
            ref.upMap
        );
    ref.angularVelocityMapRadPerSecond =
        glm::mix(
            a.angularVelocityMapRadPerSecond,
            b.angularVelocityMapRadPerSecond,
            u
        );
    ref.angularAccelerationFeedForwardMapRadPerSec2 =
        glm::mix(
            a.angularAccelerationFeedForwardMapRadPerSec2,
            b.angularAccelerationFeedForwardMapRadPerSec2,
            u
        );
    return out;
}

double RouteExecutionCore::angleBetween(
    const glm::dvec3& a,
    const glm::dvec3& b
) noexcept
{
    const double la = glm::length(a);
    const double lb = glm::length(b);
    if (!(std::isfinite(la) && std::isfinite(lb)) ||
        la <= 1.0e-12 ||
        lb <= 1.0e-12)
    {
        return 3.1415926535897932384626433832795;
    }
    return std::acos(
        std::clamp(
            glm::dot(a / la, b / lb),
            -1.0,
            1.0
        )
    );
}

glm::dvec3 RouteExecutionCore::normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double length = glm::length(value);
    return std::isfinite(length) && length > 1.0e-9
        ? value / length
        : fallback;
}

void RouteExecutionCore::makeBasis(
    const glm::dvec3& requestedForward,
    glm::dvec3& forward,
    glm::dvec3& right,
    glm::dvec3& up
) noexcept
{
    forward = normalizedOr(
        requestedForward,
        glm::dvec3(0.0, 0.0, -1.0)
    );
    glm::dvec3 seed =
        std::abs(forward.y) < 0.90
            ? glm::dvec3(0.0, 1.0, 0.0)
            : glm::dvec3(1.0, 0.0, 0.0);
    right = normalizedOr(
        glm::cross(forward, seed),
        glm::dvec3(1.0, 0.0, 0.0)
    );
    up = normalizedOr(
        glm::cross(right, forward),
        glm::dvec3(0.0, 1.0, 0.0)
    );
}

std::vector<AcceptedManeuverProgram>
RouteExecutionCore::buildPrograms(
    const planner::RoutePlan& plan,
    const AutopilotAgentState& initialAgent,
    LocalFlightControlLaw law,
    const ShipParams& params,
    double acceptedAtUniverseTimeSeconds,
    std::uint64_t requestSerial,
    double trackingPositionToleranceMeters,
    const glm::dvec3& routeUpReference,
    std::string* failureReason
)
{
    const auto failPrograms =
        [&](const std::string& reason)
            -> std::vector<AcceptedManeuverProgram>
        {
            if (failureReason)
                *failureReason = reason;
            return {};
        };

    if (!plan.valid())
        return failPrograms("planner-route-invalid-at-follower-boundary");
    if (plan.executionGates.size() < 2)
        return failPrograms("planner-route-has-too-few-execution-gates");
    if (!std::isfinite(acceptedAtUniverseTimeSeconds))
        return failPrograms("accepted-time-non-finite");
    if (requestSerial == 0)
        return failPrograms("request-serial-zero");

    const auto executionAuthority =
        makeManeuverExecutionAuthority(params, law);
    const double feedbackReserve =
        executionAuthority.feedbackReserveMps2;

    // Factual capability, zero is zero: no invented minimum speed or
    // angular authority; without them there is no executable program.
    const auto capability =
        makeNavigationControlCapability(params, law);
    if (!(capability.controlledSpeedLimitMps > 0.0) ||
        !(capability.angularScalarRadPerSec2 > 0.0) ||
        !(capability.maxAngularSpeedRadPerSec > 0.0))
        return failPrograms("capability-unavailable: no speed or attitude authority");

    world::navigation::NavigationVehicleProfile vehicle;
    vehicle.collisionRadiusMeters = 0.0;
    vehicle.preferredClearanceMeters = 0.0;
    vehicle.maxSpeedMps = capability.controlledSpeedLimitMps;
    vehicle.maxForwardAccelerationMps2 =
        executionAuthority.forwardAccelerationMps2;
    vehicle.maxBrakingAccelerationMps2 =
        executionAuthority.brakingAccelerationMps2;
    vehicle.maxLateralAccelerationMps2 =
        executionAuthority.lateralAccelerationMps2;
    vehicle.maxAngularVelocityRadPerSecond =
        capability.maxAngularSpeedRadPerSec;
    vehicle.maxAngularAccelerationRadPerSecond2 =
        capability.angularScalarRadPerSec2 * 0.90;

    world::navigation::TrajectoryGenerationRequest trajectoryRequest;
    trajectoryRequest.systemId = 0;
    trajectoryRequest.frameId = "client-docking";
    trajectoryRequest.startUniverseTimeSeconds =
        acceptedAtUniverseTimeSeconds;
    trajectoryRequest.universeTimeScale = 1.0;
    trajectoryRequest.pathGeometryAlreadyAuthored = true;
    trajectoryRequest.vehicle = vehicle;
    trajectoryRequest.initialVelocityMps =
        initialAgent.velocityMapMetersPerSecond;
    trajectoryRequest.initialAccelerationMps2 = glm::dvec3(0.0);
    trajectoryRequest.hasInitialOrientation = true;
    trajectoryRequest.initialForward = initialAgent.forwardMap;
    trajectoryRequest.initialUp = initialAgent.upMap;
    trajectoryRequest.hasInitialAngularVelocity = true;
    trajectoryRequest.initialAngularVelocityRadPerSecond =
        glm::dvec3(
            initialAgent.pitchRateRadPerSec,
            initialAgent.yawRateRadPerSec,
            initialAgent.rollRateRadPerSec
        );
    trajectoryRequest.hasRouteUpReference = true;
    trajectoryRequest.routeUpReference =
        glm::length(routeUpReference) > 1.0e-9
            ? normalizedOr(routeUpReference, initialAgent.upMap)
            : initialAgent.upMap;
    trajectoryRequest.hasTerminalVelocity = true;
    trajectoryRequest.terminalVelocityMps = glm::dvec3(0.0);

    trajectoryRequest.pathPointsMeters.reserve(
        plan.executionGates.size()
    );
    trajectoryRequest.pointSpeedConstraints.reserve(
        plan.executionGates.size() +
        plan.routeCurves.size()
    );
    std::vector<double> executionGateProgressMeters;
    executionGateProgressMeters.reserve(
        plan.executionGates.size()
    );

    double sourceProgressMeters = 0.0;
    for (std::size_t i = 0;
         i < plan.executionGates.size();
         ++i)
    {
        const auto& gate = plan.executionGates[i];
        trajectoryRequest.pathPointsMeters.push_back(
            gate.positionMeters
        );

        if (i > 0)
        {
            sourceProgressMeters += glm::length(
                gate.positionMeters -
                plan.executionGates[i - 1].positionMeters
            );
        }

        executionGateProgressMeters.push_back(
            sourceProgressMeters
        );

        world::navigation::TrajectoryPointSpeedConstraint limit;
        limit.sourcePathProgressMeters = sourceProgressMeters;
        limit.maxSpeedMps = std::min(
            vehicle.maxSpeedMps,
            std::max(0.0, gate.speedMps)
        );
        trajectoryRequest.pointSpeedConstraints.push_back(limit);
    }

    if (law == LocalFlightControlLaw::Newtonian &&
        plan.routeCurves.size() >= 2 &&
        executionGateProgressMeters.size() ==
            plan.executionGates.size())
    {
        // The planner rounds Newtonian corners where a fillet fits; a
        // corner it could not round stays a real tangent discontinuity,
        // which no finite lateral capability flies through, so the
        // profile arrives stopped there. Joins whose direction differs
        // by less than half a degree (numerical arc/line seams, merged
        // near-collinear points) are flown through.
        constexpr double FlyThroughJoinRad =
            0.5 * 3.14159265358979323846 / 180.0;
        const double TangentEqualityEpsilon2 =
            2.0 - 2.0 * std::cos(FlyThroughJoinRad);

        for (std::size_t curveIndex = 0;
             curveIndex + 1 < plan.routeCurves.size();
             ++curveIndex)
        {
            const auto& before = plan.routeCurves[curveIndex];
            const auto& after = plan.routeCurves[curveIndex + 1];

            const glm::dvec3 beforeTangent =
                normalizedOr(
                    before.tangentAtProgress(
                        before.endProgressMeters
                    ),
                    before.endForward
                );
            const glm::dvec3 afterTangent =
                normalizedOr(
                    after.tangentAtProgress(
                        after.startProgressMeters
                    ),
                    after.startForward
                );

            if (glm::length2(
                    beforeTangent - afterTangent
                ) <= TangentEqualityEpsilon2)
            {
                continue;
            }

            const glm::dvec3 joinPosition =
                0.5 * (
                    before.endMeters +
                    after.startMeters
                );

            std::size_t nearestGate = 0;
            double nearestDistance2 =
                std::numeric_limits<double>::infinity();

            for (std::size_t gateIndex = 0;
                 gateIndex < plan.executionGates.size();
                 ++gateIndex)
            {
                const glm::dvec3 delta =
                    plan.executionGates[gateIndex].
                        positionMeters -
                    joinPosition;
                const double distance2 =
                    glm::dot(delta, delta);
                if (distance2 < nearestDistance2)
                {
                    nearestDistance2 = distance2;
                    nearestGate = gateIndex;
                }
            }

            if (nearestGate == 0 ||
                nearestGate + 1 >=
                    plan.executionGates.size())
            {
                continue;
            }

            world::navigation::
                TrajectoryPointSpeedConstraint stop;
            stop.sourcePathProgressMeters =
                executionGateProgressMeters[nearestGate];
            stop.maxSpeedMps = 0.0;
            trajectoryRequest.pointSpeedConstraints.
                push_back(stop);
        }
    }

    // Newtonian speed tolerance corridor: an initial speed inside the
    // tolerance band of the route's speed limit at the start IS that
    // controlled speed for the strategic profile (400.5 m/s on a 400 m/s
    // route is not a braking manoeuvre). Beyond the band the measured
    // speed stays authoritative and the generator brakes it.
    if (law == LocalFlightControlLaw::Newtonian &&
        !trajectoryRequest.pointSpeedConstraints.empty())
    {
        const double startLimit =
            trajectoryRequest.pointSpeedConstraints.front().maxSpeedMps;
        const double initialSpeed =
            glm::length(trajectoryRequest.initialVelocityMps);
        if (initialSpeed > startLimit && startLimit > 1.0e-6 &&
            !NewtonianSpeedTolerancePolicy::beyondBand(
                initialSpeed, startLimit))
            trajectoryRequest.initialVelocityMps *=
                startLimit / initialSpeed;
    }

    constexpr int MaximumRefinementAttempts = 6;
    std::string lastRefinementDiagnostic;
    for (int attempt = 0;
         attempt < MaximumRefinementAttempts;
         ++attempt)
    {
        const auto trajectory =
            world::navigation::TrajectoryGenerator::generate(
                trajectoryRequest
            );
        if (!trajectory.ready())
        {
            return failPrograms(
                "trajectory-generation-failed attempt=" +
                std::to_string(attempt) +
                " status=" +
                std::to_string(
                    static_cast<int>(trajectory.trajectory.status)
                ) +
                " message=\"" +
                trajectory.trajectory.message +
                "\"" +
                " guide_points=" +
                std::to_string(
                    trajectory.diagnostics.executionGuidePoints
                ) +
                " max_curvature=" +
                std::to_string(
                    trajectory.diagnostics.maxCurvaturePerMeter
                ) +
                " initial_along_mps=" +
                std::to_string(
                    trajectory.diagnostics.initialAlongPathSpeedMps
                ) +
                " initial_cross_mps=" +
                std::to_string(
                    trajectory.diagnostics.initialCrossTrackSpeedMps
                )
            );
        }

        AcceptedManeuverProgramBuilder::Request build;
        build.trajectory = &trajectory.trajectory;
        build.shipPhysics = &params;
        build.controlLaw = law;
        build.referenceMode =
            AcceptedManeuverProgram::ReferenceMode::SpatialCorridor;
        build.objectiveRevision = requestSerial;
        build.firstProgramRevision = 1;
        build.capabilityRevision = requestSerial;
        build.mapRevision = requestSerial;
        build.mapSourceRevision = requestSerial;
        build.spaceRevision = requestSerial;
        build.spaceSourceRevision = requestSerial;
        build.minimumClearanceMeters = 0.0;
        build.hasInitialAngularVelocity = true;
        build.initialAngularVelocityMapRadPerSec =
            trajectoryRequest.initialAngularVelocityRadPerSecond;
        build.policy.trackingPositionErrorMeters =
            std::max(5.0, trackingPositionToleranceMeters);
        build.policy.trackingLinearVelocityErrorMps = 10.0;
        build.policy.trackingForwardAngleErrorRad =
            1.3962634015954636; // 80 deg: recover, do not abort.
        build.policy.trackingAngularVelocityErrorRadPerSec = 2.0;
        build.policy.alongTrackPositionDeadbandMeters = 10.0;
        build.policy.alongTrackSpeedDeadbandMps = 1.0;
        build.policy.linearFeedbackReserveMps2 = feedbackReserve;
        build.policy.angularFeedbackReserveRadPerSec2 =
            std::min(
                0.5,
                game::ship::angularAccelerationLimitRadPerSec2(params) *
                    0.15
            );

        auto accepted =
            AcceptedManeuverProgramBuilder::build(build);
        if (accepted.valid && !accepted.pages.empty())
            return accepted.pages;

        if (accepted.feedback.disposition !=
            AcceptedManeuverProgramBuilder::
                ValidationDisposition::NeedsRefinement)
        {
            return failPrograms(
                "accepted-program-rejected: " +
                (!accepted.feedback.message.empty()
                    ? accepted.feedback.message
                    : accepted.failureReason) +
                " page=" +
                std::to_string(accepted.feedback.pageIndex) +
                " sample=" +
                std::to_string(accepted.feedback.sampleIndex) +
                " required=" +
                std::to_string(accepted.feedback.requiredValue) +
                " available=" +
                std::to_string(accepted.feedback.availableValue)
            );
        }

        const double scale = std::clamp(
            accepted.feedback.recommendedScale,
            0.50,
            0.98
        );

        lastRefinementDiagnostic =
            " attempt=" + std::to_string(attempt) +
            " refinement=" +
                std::to_string(
                    static_cast<int>(accepted.feedback.refinement)
                ) +
            " message=\"" +
                (!accepted.feedback.message.empty()
                    ? accepted.feedback.message
                    : accepted.failureReason) +
                "\"" +
            " page=" +
                std::to_string(accepted.feedback.pageIndex) +
            " sample=" +
                std::to_string(accepted.feedback.sampleIndex) +
            " required=" +
                std::to_string(accepted.feedback.requiredValue) +
            " available=" +
                std::to_string(accepted.feedback.availableValue) +
            " recommended_scale=" +
                std::to_string(accepted.feedback.recommendedScale) +
            " applied_scale=" +
                std::to_string(scale) +
            " vehicle_max_speed=" +
                std::to_string(trajectoryRequest.vehicle.maxSpeedMps);
        trajectoryRequest.vehicle.maxSpeedMps =
            std::max(
                0.5,
                trajectoryRequest.vehicle.maxSpeedMps * scale
            );
        // The first route station is a measured initial state,
        // not a future planning constraint. Refinement must never lower
        // it below the velocity with which the craft actually enters the
        // accepted route; doing so creates an artificial acceleration
        // discontinuity at sample zero and can make "brake earlier"
        // refinement self-defeating.
        //
        // Only future speed constraints are refinable. The generator
        // already treats initialVelocityMps as authoritative and will
        // brake an initial overspeed over physically available distance.
        for (std::size_t limitIndex = 1;
             limitIndex <
                 trajectoryRequest.pointSpeedConstraints.size();
             ++limitIndex)
        {
            auto& limit =
                trajectoryRequest.pointSpeedConstraints[limitIndex];
            limit.maxSpeedMps =
                std::max(0.0, limit.maxSpeedMps * scale);
        }
    }

    return failPrograms(
        "accepted-program-refinement-exhausted" +
        lastRefinementDiagnostic
    );
}

} // namespace game::navigation::autopilot
