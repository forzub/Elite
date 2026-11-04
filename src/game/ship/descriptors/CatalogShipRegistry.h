#pragma once

#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "src/game/ship/descriptors/GenericShipDescriptorBuilder.h"

// Process registry of catalog ships: every design of ShipDesignCatalog built
// ONCE by the generic builder and kept for the process lifetime (descriptors
// are referenced by ShipCore, snapshots and the client). The passport id is
// the stable runtime identity; spawn, replication and the client resolve a
// catalog ship here, never through an ObjectType.
namespace game::ship::descriptors
{
class CatalogShipRegistry
{
public:
    [[nodiscard]] static CatalogShipRegistry& instance()
    {
        static auto* registry = new CatalogShipRegistry();   // never destroyed
        return *registry;
    }

    // Descriptor of a passport id; nullptr if the catalog has no such ship
    // or the generic builder rejects it (lastError() says why).
    [[nodiscard]] const ShipDescriptor* find(const std::string& passportId)
    {
        std::lock_guard lock(m_mutex);
        buildLocked();
        for (const auto& e : m_entries)
            if (e.passportId == passportId)
                return e.descriptor ? &*e.descriptor : nullptr;
        return nullptr;
    }

    [[nodiscard]] std::vector<std::string> passportIds()
    {
        std::lock_guard lock(m_mutex);
        buildLocked();
        std::vector<std::string> out;
        for (const auto& e : m_entries)
            if (e.descriptor)
                out.push_back(e.passportId);
        return out;
    }

    [[nodiscard]] std::string errorOf(const std::string& passportId)
    {
        std::lock_guard lock(m_mutex);
        buildLocked();
        for (const auto& e : m_entries)
            if (e.passportId == passportId)
                return e.error;
        return "unknown passport " + passportId;
    }

private:
    struct Entry
    {
        std::string passportId;
        std::optional<ShipDescriptor> descriptor;
        std::string error;
    };

    void buildLocked()
    {
        if (m_built)
            return;
        m_built = true;
        for (const auto& design : ShipDesignCatalog::instance().ships())
        {
            Entry e;
            e.passportId = design.id;
            e.descriptor = buildGenericShipDescriptor(design, e.error);
            m_entries.push_back(std::move(e));   // deque: element addresses stay valid
        }
    }

    std::mutex m_mutex;
    bool m_built = false;
    std::deque<Entry> m_entries;
};

[[nodiscard]] inline const ShipDescriptor* findCatalogShip(const std::string& passportId)
{
    return CatalogShipRegistry::instance().find(passportId);
}
} // namespace game::ship::descriptors
