#pragma once

// Registered actuator topology of a real installed loadout: the six-model
// slot structure (plant::canonical) with THIS hull's own mount coordinates
// from its design passport, plus its 24-nozzle Newtonian RCS bank geometry.
//
// ShipParams carries only a 64-bit key (actuatorTopologyKey): ShipParams is
// copied freely and written raw into autopilot case files, so it must never
// hold a pointer. Key 0 or an unknown key means the canonical coordinates.
// Every lever-arm consumer (r x F of mains and nozzles, allocators, control
// matrices, capability) reads positions through actuatorSlots() /
// rcsNozzleBank(); structure-only consumers may keep physicalControlSlots().
// Entries are registered while descriptors are built and are never removed.

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/plant/CanonicalActuatorLayouts.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"

namespace game::ship::plant
{
struct RcsNozzleGeometry
{
    glm::dvec3 positionBody {0.0};
    glm::dvec3 forceDirectionBody {0.0};
};
using RcsNozzleBank = std::array<RcsNozzleGeometry, 24>;

struct ActuatorTopology
{
    PhysicalControlModelId model = PhysicalControlModelId::AssistedI;
    std::vector<EngineSlotDefinition> slots;
    RcsNozzleBank rcs {};
};

[[nodiscard]] inline const RcsNozzleBank& canonicalRcsNozzleBank() noexcept
{
    static const RcsNozzleBank bank = [] {
        RcsNozzleBank out {};
        const auto& mounts = canonical::newtonianRcsNozzles();
        for (std::size_t i = 0; i < out.size(); ++i)
            out[i] = {mounts[i].positionBody, mounts[i].forceDirectionBody};
        return out;
    }();
    return bank;
}

// FNV-1a; never 0 (0 is the canonical topology).
[[nodiscard]] constexpr std::uint64_t actuatorTopologyKey(std::string_view id) noexcept
{
    std::uint64_t h = 1469598103934665603ull;
    for (const char c : id)
    {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ull;
    }
    return h == 0 ? 1 : h;
}

namespace detail
{
struct ActuatorTopologyRegistry
{
    std::shared_mutex mutex;
    std::unordered_map<std::uint64_t, std::unique_ptr<const ActuatorTopology>> entries;
    // Superseded entries stay alive: a reader may still hold their reference.
    std::vector<std::unique_ptr<const ActuatorTopology>> retired;
};

[[nodiscard]] inline ActuatorTopologyRegistry& actuatorTopologyRegistry()
{
    static auto* registry = new ActuatorTopologyRegistry(); // never destroyed
    return *registry;
}
} // namespace detail

inline void registerActuatorTopology(std::uint64_t key, ActuatorTopology topology)
{
    if (key == 0)
        return;
    auto& r = detail::actuatorTopologyRegistry();
    std::unique_lock lock(r.mutex);
    auto& slot = r.entries[key];
    if (slot)
        r.retired.push_back(std::move(slot));
    slot = std::make_unique<const ActuatorTopology>(std::move(topology));
}

[[nodiscard]] inline const ActuatorTopology* findActuatorTopology(std::uint64_t key)
{
    if (key == 0)
        return nullptr;
    auto& r = detail::actuatorTopologyRegistry();
    std::shared_lock lock(r.mutex);
    const auto it = r.entries.find(key);
    return it == r.entries.end() ? nullptr : it->second.get();
}

// Slots with this ship's mount coordinates; the canonical table when the
// key is unknown or registered for another model (diagnostic re-layouts).
[[nodiscard]] inline const std::vector<EngineSlotDefinition>& actuatorSlots(
    const ShipParams& physics,
    PhysicalControlModelId model
)
{
    if (const auto* t = findActuatorTopology(physics.actuatorTopologyKey))
        if (t->model == model && t->slots.size() == physicalControlSlots(model).size())
            return t->slots;
    return physicalControlSlots(model);
}

[[nodiscard]] inline const RcsNozzleBank& rcsNozzleBank(std::uint64_t topologyKey)
{
    if (const auto* t = findActuatorTopology(topologyKey))
        return t->rcs;
    return canonicalRcsNozzleBank();
}

[[nodiscard]] inline const RcsNozzleBank& rcsNozzleBank(const ShipParams& physics)
{
    return rcsNozzleBank(physics.actuatorTopologyKey);
}
} // namespace game::ship::plant
