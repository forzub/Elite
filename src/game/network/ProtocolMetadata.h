#pragma once

#include <cstdint>

namespace game::network
{
struct SnapshotMetadata
{
    std::uint64_t serverTick = 0;
    double serverTimeSeconds = 0.0;
    double universeTimeSeconds = 0.0;
};

struct CatalogMetadata
{
    std::uint64_t catalogRevision = 0;
};
}
