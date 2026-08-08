#pragma once

#include <cstdint>

namespace game::network
{
struct SnapshotMetadata
{
    std::uint64_t serverTick = 0;
    double serverTimeSeconds = 0.0;
    double universeTimeSeconds = 0.0;

    // Fence between discontinuous universe-time branches. Server time remains
    // monotonic across a debug rewind, so consumers must never infer timeline
    // continuity from serverTick/serverTimeSeconds alone.
    std::uint64_t universeTimelineRevision = 1;
};

struct CatalogMetadata
{
    std::uint64_t catalogRevision = 0;
};
}
