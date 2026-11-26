#pragma once

// Snapshot SIZE instrumentation - not codec logic (the top-level codec delegates every field to the schema;
// tests/architecture_contracts/check_wire_data_schema.py forbids it to name nested world fields). What makes a
// snapshot big: each part encoded alone.

#include <algorithm>
#include <iostream>
#include <cstdint>
#include <utility>
#include <vector>

#include "src/game/network/WireDataSchema.h"

namespace game::network::wire
{

template <class V>
inline std::size_t encodedSizeOf(const V& part)
{
    WireWriter w;
    (void)binary::encodeValue(w, part);
    return w.take().size();
}

// the top-level parts of one snapshot, raw (uncompressed) bytes
struct SnapshotPartSizes
{
    std::size_t ships = 0, objects = 0, hubs = 0, signals = 0, session = 0;
};

inline SnapshotPartSizes snapshotPartSizes(const SimulationSnapshot& value)
{
    return {encodedSizeOf(value.ships), encodedSizeOf(value.objects), encodedSizeOf(value.hubs),
            encodedSizeOf(value.signals), encodedSizeOf(value.session)};
}

inline void reportOversizeSnapshot(const SimulationSnapshot& value, std::size_t payloadBytes)
{
    const std::size_t outPayloadSize = payloadBytes;
        const auto sizeOf = [](const auto& part) { return encodedSizeOf(part); };
        {
            // the heaviest objects (type, graph share)
            std::vector<std::pair<std::size_t, std::size_t>> sizes;
            for (std::size_t i = 0; i < value.objects.size(); ++i)
                sizes.emplace_back(sizeOf(value.objects[i]), i);
            std::sort(sizes.rbegin(), sizes.rend());
            for (std::size_t k = 0; k < sizes.size() && k < 6; ++k)
            {
                const auto& o = value.objects[sizes[k].second];
                std::cerr << "[Wire]   object " << o.id.value << " type " << static_cast<int>(o.type) << " '" << o.displayName
                          << "': " << sizes[k].first << " B, graph " << sizeOf(o.graph) << " B (modules "
                          << o.graph.modules.size() << " = " << sizeOf(o.graph.modules) << " B, links "
                          << o.graph.structuralLinks.size() << " = " << sizeOf(o.graph.structuralLinks) << " B, debug hit "
                          << o.graph.debugHitVolumes.size() << " = " << sizeOf(o.graph.debugHitVolumes) << " B, assembly "
                          << sizeOf(o.graph.assemblyModules) << " B)" << std::endl;
            }
            std::size_t over100k = 0, total100k = 0;
            for (const auto& s : sizes)
                if (s.first > 100000)
                {
                    ++over100k;
                    total100k += s.first;
                }
            std::cerr << "[Wire]   objects over 100 KB: " << over100k << ", " << total100k << " B together" << std::endl;
        }
        std::size_t biggestHub = 0, biggestHubIndex = 0;
        for (std::size_t i = 0; i < value.hubs.size(); ++i)
        {
            const std::size_t s = sizeOf(value.hubs[i]);
            if (s > biggestHub)
            {
                biggestHub = s;
                biggestHubIndex = i;
            }
        }
        std::cerr << "[Wire] snapshot: " << outPayloadSize << " bytes over the " << MaxWirePayloadBytes
                  << " limit - ships " << value.ships.size() << " = " << sizeOf(value.ships) << " B, objects "
                  << value.objects.size() << " = " << sizeOf(value.objects) << " B, hubs " << value.hubs.size() << " = "
                  << sizeOf(value.hubs) << " B (biggest #" << biggestHubIndex << " " << biggestHub << " B), signals "
                  << sizeOf(value.signals) << " B, session " << sizeOf(value.session) << " B" << std::endl;
}

} // namespace game::network::wire
