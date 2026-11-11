#pragma once

#include <cstddef>
#include <string>

namespace core::dev
{
/*
    DEV memory ledger: every cache that holds celestial resources reports
    its current size here (bytes + entries, CPU or GPU); with
    ELITE_DEV_CELESTIAL_MEMORY=<seconds> a background thread prints one
    block per interval to stderr:

        [CelestialMemory] process working set 1834 MB, private 2210 MB
        [CelestialMemory]   cpu flight.fields             412.3 MB     14 entries
        [CelestialMemory]   gpu map.generated_textures     88.0 MB      8 entries

    Owners call memoryLedgerSet() when their content changes (or once a
    frame - a map write under a mutex). Inert without the env var (one
    cached getenv per call). Audit: docs/CELESTIAL_RESOURCE_AUDIT.md.
    Implementation: DevMemoryLedger.cpp (library EliteCelestialSurface,
    linked by the client and the server).
*/
bool memoryLedgerEnabled();

// name: "cpu <owner>" or "gpu <owner>" (the prefix decides the total it adds to).
void memoryLedgerSet(const std::string& name, std::size_t bytes, std::size_t entries);

// Prints the block now (also called by the interval thread).
void memoryLedgerPrint();
} // namespace core::dev
