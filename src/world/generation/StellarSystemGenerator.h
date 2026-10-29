#pragma once

#include <cstdint>
#include <string>

#include "src/world/generation/UniverseDescriptors.h"

namespace world::generation
{

/*
    One deterministic pass from a system input (identity, position, stars,
    hierarchy, observational anchors) to a complete physical system.

    Seeds:
      catalog system    SystemSeed = derive(UniverseSeed, "catalog_system", id)
      procedural system SystemSeed = derive(GalacticCellSeed(cell), "procedural_system", index)
    so a catalog system keeps its content when its position is corrected and
    adding procedural systems never changes an existing one.
*/

std::uint64_t catalogSystemSeed(std::uint64_t universeSeed, int systemId);
std::uint64_t galacticCellSeed(std::uint64_t universeSeed, long long cx, long long cy, long long cz);

SystemDescriptor generateStellarSystem(
    const SystemInput& input,
    std::uint64_t universeSeed,
    const UniverseGenerationVersions& versions = CurrentGenerationVersions);

// Runtime body id leaf, compatible with the legacy catalog ids
// ("system_0.Sol.Земля"): spaces and path separators become '_'.
std::string safeIdPart(const std::string& name);

// ASCII file-name slug (for per-body files).
std::string fileSlug(const std::string& name, const std::vector<std::string>& alternatives);

} // namespace world::generation
