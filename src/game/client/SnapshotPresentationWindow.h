#pragma once

#include <algorithm>
#include <cstddef>
#include <cmath>

namespace game::client
{

struct SnapshotPresentationWindow
{
    bool hasSnapshots = false;
    bool clampedToOldest = false;
    bool clampedToNewest = false;
    bool hasInterpolationBracket = false;

    double requestedRenderTimeSeconds = 0.0;
    double renderTimeSeconds = 0.0;
    double oldestSnapshotTimeSeconds = 0.0;
    double newestSnapshotTimeSeconds = 0.0;

    std::size_t olderIndex = 0;
    std::size_t newerIndex = 0;
    double interpolationAlpha = 0.0;
};

/*
    Resolve the authoritative snapshot interval used for one client render frame.

    The caller supplies the single presentation playhead selected by
    ClientPresentationClock. This helper owns only sampling mechanics:

      * clamp to the history that actually exists;
      * select one adjacent snapshot pair;
      * calculate one interpolation alpha shared by every remote object and
        reference-frame presentation sample in that frame.

    Keeping this logic independent from object rendering prevents different
    presentation branches from silently choosing different epochs.
*/
template <typename SnapshotContainer, typename TimeAccessor>
SnapshotPresentationWindow resolveSnapshotPresentationWindow(
    const SnapshotContainer& snapshots,
    double requestedRenderTimeSeconds,
    TimeAccessor timeOf
) noexcept
{
    SnapshotPresentationWindow out;
    out.requestedRenderTimeSeconds =
        std::isfinite(requestedRenderTimeSeconds)
            ? std::max(0.0, requestedRenderTimeSeconds)
            : 0.0;
    out.renderTimeSeconds = out.requestedRenderTimeSeconds;

    if (snapshots.empty())
        return out;

    out.hasSnapshots = true;
    out.oldestSnapshotTimeSeconds = timeOf(snapshots.front());
    out.newestSnapshotTimeSeconds = timeOf(snapshots.back());

    if (!std::isfinite(out.oldestSnapshotTimeSeconds) ||
        !std::isfinite(out.newestSnapshotTimeSeconds) ||
        out.newestSnapshotTimeSeconds < out.oldestSnapshotTimeSeconds)
    {
        out.hasSnapshots = false;
        return out;
    }

    out.clampedToOldest =
        out.renderTimeSeconds < out.oldestSnapshotTimeSeconds;
    out.clampedToNewest =
        out.renderTimeSeconds > out.newestSnapshotTimeSeconds;

    out.renderTimeSeconds =
        std::clamp(
            out.renderTimeSeconds,
            out.oldestSnapshotTimeSeconds,
            out.newestSnapshotTimeSeconds
        );

    if (snapshots.size() < 2)
        return out;

    for (std::size_t i = 0; i + 1 < snapshots.size(); ++i)
    {
        const double olderTime = timeOf(snapshots[i]);
        const double newerTime = timeOf(snapshots[i + 1]);

        if (!std::isfinite(olderTime) || !std::isfinite(newerTime))
            continue;

        if (olderTime <= out.renderTimeSeconds &&
            newerTime >= out.renderTimeSeconds)
        {
            const double span = newerTime - olderTime;
            if (span <= 0.0)
                return out;

            out.hasInterpolationBracket = true;
            out.olderIndex = i;
            out.newerIndex = i + 1;
            out.interpolationAlpha =
                std::clamp(
                    (out.renderTimeSeconds - olderTime) / span,
                    0.0,
                    1.0
                );
            return out;
        }
    }

    return out;
}

} // namespace game::client
