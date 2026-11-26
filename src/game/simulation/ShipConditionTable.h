#pragma once

// The CONDITION of every ship in the universe, compact, on the server (author 2026-11-26): "otherwise every ship but
// the player's has perfect health - we need to simulate their ability to repair, the lack of resources, no access to
// spare parts". Corporate ships: steady, understood wear (garages, scheduled maintenance); private ships: up to the
// owner and his wealth; in a war everyone may suffer from a lack of parts; pirates always do, except large
// mafia-like organisations.
//
// One fixed-size cell per ship in ONE contiguous array: a ship's cell is at base + slot * sizeof(Cell); the slot rides
// in the ship's ShipSimulationLifecycleState (shell <-> full ship both carry it), so access is direct, no lookup.
// A section = one module of the ship (the module order of a descriptor is deterministic); 8 bytes per section:
// integrity + the six EquipmentDamageState causes + the module state. A ship that never was materialized has no
// per-module picture yet (sectionCount 0): its cell holds ONE uniform section, spread per module deterministically
// when it materializes.
//
// OWNERSHIP: while a ship is materialized its ShipCore is the truth (the cell is stale, liveInShipCore = 1); when it
// becomes a shell the cell is written from the modules and IS the truth; on materialize the cell is applied back.
// The maintenance pass (advanceShipUpkeep) only touches cells whose truth they are.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "src/game/ship/ShipRoleType.h"
#include "src/world/modules/ObjectModuleState.h"

namespace game::simulation
{

// who keeps the ship running, and so how well (the doctrine above)
enum class ShipUpkeepRegime : std::uint8_t
{
    Private = 0,     // the owner's wealth decides
    Corporate = 1,   // garages, scheduled maintenance: steady, understood wear
    State = 2,       // military / security: supplied, until a war eats the parts
    Syndicate = 3,   // large mafia-like organisations: their own supply chains
    Pirate = 4,      // always short of parts
};

inline const char* upkeepRegimeName(ShipUpkeepRegime r) noexcept
{
    switch (r)
    {
    case ShipUpkeepRegime::Corporate: return "corporate";
    case ShipUpkeepRegime::State: return "state";
    case ShipUpkeepRegime::Syndicate: return "syndicate";
    case ShipUpkeepRegime::Pirate: return "pirate";
    default: return "private";
    }
}

struct ShipSectionCondition
{
    std::uint8_t integrity = 255;   // module health / maxHealth
    std::uint8_t laser = 0;         // EquipmentDamageState causes, 0..1 -> 0..255
    std::uint8_t impact = 0;
    std::uint8_t thermal = 0;
    std::uint8_t radiation = 0;
    std::uint8_t wear = 0;
    std::uint8_t internalFailure = 0;
    std::uint8_t state = 0;         // world::modules::ObjectModuleRuntimeState
};
static_assert(sizeof(ShipSectionCondition) == 8);

struct ShipConditionCell
{
    static constexpr std::size_t kMaxSections = 64;

    std::uint64_t entityId = 0;          // 0 = a free cell
    std::uint8_t sectionCount = 0;       // 0 = uniform (never materialized): sections[0] stands for every module
    ShipUpkeepRegime regime = ShipUpkeepRegime::Private;
    std::uint8_t wealth = 128;           // the owner's means (private ships), 0..255
    std::uint8_t parts = 255;            // spare parts on hand, 0..255 (a full set = 255)
    std::uint8_t liveInShipCore = 0;     // 1: materialized - the ShipCore is the truth, this cell is stale
    std::uint8_t abandoned = 0;         // a wreck: nobody keeps it (no upkeep, no wear)
    std::uint8_t reserved[2] {};
    std::uint32_t revision = 0;
    float upkeepClockSeconds = 0.0f;     // condition time not yet advanced by the upkeep pass
    // the ship's LOAD (ShipLoadState: a persistent fact - cargo aboard must not vanish in a shell)
    float cargoTonnes = 0.0f;
    std::int16_t containers = -1;
    std::uint16_t persons = 0;
    std::uint8_t fluids = 0;             // 0..1 -> 0..255
    std::uint8_t consumables = 0;
    std::uint8_t hasLoad = 0;            // 1: captured from a materialized ship (else the descriptor's load stands)
    std::uint8_t reservedLoad = 0;
    std::array<ShipSectionCondition, kMaxSections> sections {};
};

inline std::uint8_t toByte01(double v) noexcept
{
    if (!std::isfinite(v))
        return 0;
    return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
}
inline double fromByte01(std::uint8_t b) noexcept { return static_cast<double>(b) / 255.0; }
// rounding with a deterministic dither u in [0,1): slow processes (wear of 1e-4 / h at a 10 s step) still accumulate
// correctly ON AVERAGE instead of rounding away to nothing
inline std::uint8_t toByte01Dithered(double v, double u) noexcept
{
    if (!std::isfinite(v))
        return 0;
    const double x = std::clamp(v, 0.0, 1.0) * 255.0;
    const double f = std::floor(x);
    return static_cast<std::uint8_t>(std::min(255.0, f + ((x - f) > u ? 1.0 : 0.0)));
}

// deterministic per-ship randomness (no global RNG state): splitmix64
inline double conditionHash01(std::uint64_t a, std::uint64_t b) noexcept
{
    std::uint64_t z = a * 0x9E3779B97F4A7C15ull + b + 0x632BE59BD9B4E019ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return static_cast<double>(z >> 11) * (1.0 / 9007199254740992.0);
}

// The regime from what NPC ships carry today (no NPC has an owner actor yet): ONE place, replaced by the owner
// actor's kind (ActorKind Corporation / State / PirateUnion) once NPC ownership exists.
inline ShipUpkeepRegime upkeepRegimeForRole(ShipRoleType role, std::uint64_t entityId) noexcept
{
    switch (role)
    {
    case ShipRoleType::Military:
    case ShipRoleType::Security: return ShipUpkeepRegime::State;
    case ShipRoleType::Transport:
    case ShipRoleType::Passenger: return ShipUpkeepRegime::Corporate;
    case ShipRoleType::Trader: return conditionHash01(entityId, 11) < 0.55 ? ShipUpkeepRegime::Corporate
                                                                           : ShipUpkeepRegime::Private;
    case ShipRoleType::Pirate: return conditionHash01(entityId, 12) < 0.15 ? ShipUpkeepRegime::Syndicate
                                                                           : ShipUpkeepRegime::Pirate;
    default: return ShipUpkeepRegime::Private;
    }
}

// What a regime does to a ship: the starting condition range and the upkeep rates. ONE table to tune.
// Rates per hour of game time. Real reference: an airliner gets a light check every ~500-800 flight hours and a
// heavy check every 1.5-2 years; engines run ~20 000 h between overhauls - wear itself is slow, what differs
// between owners is how fast DAMAGE is repaired and whether there are parts to do it.
struct ShipUpkeepProfile
{
    double startWearMin, startWearMax;      // wear01 at spawn
    double startDamageMax;                  // impact/laser damage at spawn (an old fight not yet repaired)
    double startPartsMin, startPartsMax;
    double wearPerHour;                     // operating wear growth
    double repairPerHour;                   // damage (and integrity) restored per hour, given parts
    double partsPerHour;                    // spare parts restocked per hour (x the war supply factor)
    double partsPerRepair;                  // parts consumed per unit of damage repaired
};

inline const ShipUpkeepProfile& upkeepProfile(ShipUpkeepRegime r) noexcept
{
    static const ShipUpkeepProfile kCorporate {0.05, 0.25, 0.03, 0.80, 1.00, 0.00010, 0.050, 0.020, 0.5};
    static const ShipUpkeepProfile kState     {0.05, 0.30, 0.05, 0.70, 1.00, 0.00012, 0.060, 0.025, 0.5};
    static const ShipUpkeepProfile kPrivate   {0.10, 0.60, 0.15, 0.20, 0.90, 0.00015, 0.030, 0.010, 0.6};
    static const ShipUpkeepProfile kSyndicate {0.20, 0.50, 0.20, 0.40, 0.70, 0.00015, 0.035, 0.010, 0.6};
    static const ShipUpkeepProfile kPirate    {0.30, 0.80, 0.40, 0.00, 0.30, 0.00025, 0.015, 0.002, 0.8};
    switch (r)
    {
    case ShipUpkeepRegime::Corporate: return kCorporate;
    case ShipUpkeepRegime::State: return kState;
    case ShipUpkeepRegime::Syndicate: return kSyndicate;
    case ShipUpkeepRegime::Pirate: return kPirate;
    default: return kPrivate;
    }
}

class ShipConditionTable
{
public:
    static constexpr std::uint32_t kNoSlot = 0xFFFFFFFFu;

    std::uint32_t allocate(std::uint64_t entityId)
    {
        std::uint32_t slot;
        if (!m_free.empty())
        {
            slot = m_free.back();
            m_free.pop_back();
            m_cells[slot] = ShipConditionCell {};
        }
        else
        {
            slot = static_cast<std::uint32_t>(m_cells.size());
            m_cells.emplace_back();
        }
        m_cells[slot].entityId = entityId;
        ++m_live;
        return slot;
    }

    // releases only the cell this entity owns (a double release / a stale slot is a no-op)
    void release(std::uint32_t slot, std::uint64_t entityId)
    {
        if (slot >= m_cells.size() || m_cells[slot].entityId != entityId || entityId == 0)
            return;
        m_cells[slot].entityId = 0;
        m_free.push_back(slot);
        --m_live;
    }

    ShipConditionCell* at(std::uint32_t slot, std::uint64_t entityId) noexcept
    {
        return slot < m_cells.size() && m_cells[slot].entityId == entityId && entityId != 0 ? &m_cells[slot] : nullptr;
    }
    const ShipConditionCell* at(std::uint32_t slot, std::uint64_t entityId) const noexcept
    {
        return slot < m_cells.size() && m_cells[slot].entityId == entityId && entityId != 0 ? &m_cells[slot] : nullptr;
    }

    std::vector<ShipConditionCell>& cells() noexcept { return m_cells; }
    std::size_t liveCount() const noexcept { return m_live; }
    std::size_t bytes() const noexcept { return m_cells.capacity() * sizeof(ShipConditionCell); }

private:
    std::vector<ShipConditionCell> m_cells;
    std::vector<std::uint32_t> m_free;
    std::size_t m_live = 0;
};

// the starting condition of a ship that enters the universe (not new: it has lived before the player met it)
inline void generateStartingCondition(ShipConditionCell& cell, ShipRoleType role)
{
    const std::uint64_t id = cell.entityId;
    cell.regime = upkeepRegimeForRole(role, id);
    cell.wealth = toByte01(conditionHash01(id, 21));
    const auto& p = upkeepProfile(cell.regime);
    // a private owner's means scale everything: a rich owner keeps a ship like a corporation
    const double means = cell.regime == ShipUpkeepRegime::Private ? fromByte01(cell.wealth) : 1.0;
    const double wear = p.startWearMin + (p.startWearMax - p.startWearMin) * conditionHash01(id, 22) * (1.25 - means);
    const double damage = p.startDamageMax * conditionHash01(id, 23) * conditionHash01(id, 24) * (1.25 - means);
    cell.parts = toByte01((p.startPartsMin + (p.startPartsMax - p.startPartsMin) * conditionHash01(id, 25)) *
                          (cell.regime == ShipUpkeepRegime::Private ? 0.3 + 0.7 * means : 1.0));
    cell.sectionCount = 0;
    auto& u = cell.sections[0];
    u.wear = toByte01(wear);
    u.impact = toByte01(damage * 0.6);
    u.laser = toByte01(damage * 0.4);
    u.integrity = toByte01(1.0 - damage * 0.8);
    u.state = 0;
    ++cell.revision;
}

// the ship's modules -> the cell (the ship becomes a shell / leaves materialization: the cell becomes the truth)
inline void captureCondition(ShipConditionCell& cell, const std::vector<world::modules::ObjectModuleState>& modules)
{
    const std::size_t n = std::min(modules.size(), ShipConditionCell::kMaxSections);
    for (std::size_t i = 0; i < n; ++i)
    {
        const auto& m = modules[i];
        const auto& d = m.equipmentDamage.accumulated;
        auto& s = cell.sections[i];
        s.integrity = toByte01(m.maxHealth > 0.0f ? m.health / m.maxHealth : 1.0);
        s.laser = toByte01(d.laser01);
        s.impact = toByte01(d.impact01);
        s.thermal = toByte01(d.thermal01);
        s.radiation = toByte01(d.radiation01);
        s.wear = toByte01(d.wear01);
        s.internalFailure = toByte01(d.internalFailure01);
        s.state = static_cast<std::uint8_t>(m.state);
    }
    cell.sectionCount = static_cast<std::uint8_t>(n);
    cell.liveInShipCore = 0;
    ++cell.revision;
}

// the cell -> a freshly built ship's modules (materialize). Returns the number of modules touched.
inline std::size_t applyCondition(const ShipConditionCell& cell, std::vector<world::modules::ObjectModuleState>& modules)
{
    using world::modules::ObjectModuleRuntimeState;
    const bool uniform = cell.sectionCount == 0;
    if (!uniform && cell.sectionCount != std::min(modules.size(), ShipConditionCell::kMaxSections))
        return 0;   // the descriptor changed under the cell: keep the descriptor's state, never misapply
    const std::size_t n = uniform ? modules.size() : cell.sectionCount;
    for (std::size_t i = 0; i < n; ++i)
    {
        auto& m = modules[i];
        const auto& s = uniform ? cell.sections[0] : cell.sections[i];
        // a never-materialized ship: one uniform condition, spread per module (+-35 %, deterministic)
        const double k = uniform ? 0.65 + 0.7 * conditionHash01(cell.entityId, 1000 + i) : 1.0;
        auto& d = m.equipmentDamage.accumulated;
        d.laser01 = std::clamp(fromByte01(s.laser) * k, 0.0, 1.0);
        d.impact01 = std::clamp(fromByte01(s.impact) * k, 0.0, 1.0);
        d.thermal01 = std::clamp(fromByte01(s.thermal) * k, 0.0, 1.0);
        d.radiation01 = std::clamp(fromByte01(s.radiation) * k, 0.0, 1.0);
        d.wear01 = std::clamp(fromByte01(s.wear) * k, 0.0, 1.0);
        d.internalFailure01 = std::clamp(fromByte01(s.internalFailure) * k, 0.0, 1.0);
        const double integrity = uniform ? 1.0 - (1.0 - fromByte01(s.integrity)) * k : fromByte01(s.integrity);
        m.health = static_cast<float>(m.maxHealth * std::clamp(integrity, 0.0, 1.0));
        if (!uniform && s.state <= static_cast<std::uint8_t>(ObjectModuleRuntimeState::Hanging))
            m.state = static_cast<ObjectModuleRuntimeState>(s.state);
    }
    return n;
}

// the shell's coarse integrity (what its coarse motion may read): mean section integrity
inline double cellIntegrity01(const ShipConditionCell& cell) noexcept
{
    const std::size_t n = cell.sectionCount == 0 ? 1 : cell.sectionCount;
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        sum += fromByte01(cell.sections[i].integrity);
    return sum / static_cast<double>(n);
}

// One upkeep step for a cell whose truth it is (a shell): wear grows; parts restock (x the war supply factor);
// damage is repaired as far as the parts allow. dtHours of game time.
inline void advanceShipUpkeep(ShipConditionCell& cell, double dtHours, double partsSupplyFactor)
{
    if (cell.entityId == 0 || cell.liveInShipCore || cell.abandoned || dtHours <= 0.0)
        return;
    const auto& p = upkeepProfile(cell.regime);
    const double means = cell.regime == ShipUpkeepRegime::Private ? fromByte01(cell.wealth) : 1.0;
    double parts = fromByte01(cell.parts) + p.partsPerHour * means * std::max(0.0, partsSupplyFactor) * dtHours;
    parts = std::min(parts, 1.0);
    const std::size_t n = cell.sectionCount == 0 ? 1 : cell.sectionCount;
    std::uint64_t draw = static_cast<std::uint64_t>(cell.revision) * 1024u;
    const auto quantize = [&](double v) { return toByte01Dithered(v, conditionHash01(cell.entityId, ++draw)); };
    double repairBudget = p.repairPerHour * dtHours;
    for (std::size_t i = 0; i < n; ++i)
    {
        auto& s = cell.sections[i];
        if (s.state == static_cast<std::uint8_t>(world::modules::ObjectModuleRuntimeState::Detached) ||
            s.state == static_cast<std::uint8_t>(world::modules::ObjectModuleRuntimeState::Destroyed))
            continue;   // a lost / destroyed module needs a yard and a new unit, not field upkeep
        s.wear = quantize(fromByte01(s.wear) + p.wearPerHour * dtHours);
        // repair the worst damage first within the budget the parts allow
        for (std::uint8_t* cause : {&s.impact, &s.laser, &s.thermal, &s.radiation, &s.internalFailure})
        {
            const double have = fromByte01(*cause);
            const double possible = std::min({have, repairBudget, p.partsPerRepair > 0.0 ? parts / p.partsPerRepair : have});
            if (possible <= 0.0)
                continue;
            *cause = quantize(have - possible);
            parts -= possible * p.partsPerRepair;
            repairBudget -= possible;
        }
        const double integrity = fromByte01(s.integrity);
        const double lift = std::min({1.0 - integrity, repairBudget, p.partsPerRepair > 0.0 ? parts / p.partsPerRepair : 1.0});
        if (lift > 0.0)
        {
            s.integrity = quantize(integrity + lift);
            parts -= lift * p.partsPerRepair;
            repairBudget -= lift;
        }
    }
    cell.parts = quantize(std::max(parts, 0.0));
    ++cell.revision;
}

// the shell's coarse damage (CoarseShipDamageState: what the shell's coarse motion reads) from its cell
template <class CoarseDamage>
inline void coarseDamageFromCell(const ShipConditionCell& cell, CoarseDamage& out)
{
    const std::size_t n = cell.sectionCount == 0 ? 1 : cell.sectionCount;
    double function = 0.0;
    for (std::size_t i = 0; i < n; ++i)
    {
        const auto& s = cell.sections[i];
        const bool lost = s.state == static_cast<std::uint8_t>(world::modules::ObjectModuleRuntimeState::Detached) ||
                          s.state == static_cast<std::uint8_t>(world::modules::ObjectModuleRuntimeState::Destroyed);
        const double damage = fromByte01(s.laser) + fromByte01(s.impact) + fromByte01(s.thermal) +
                              fromByte01(s.radiation) + fromByte01(s.internalFailure) + 0.5 * fromByte01(s.wear);
        function += lost ? 0.0 : std::clamp(1.0 - damage, 0.0, 1.0);
    }
    function /= static_cast<double>(n);
    out.structuralIntegrity01 = cellIntegrity01(cell);
    // per-engine authority stays what the materialized ship measured when it had one; a cell-only ship: the mean
    if (cell.sectionCount == 0)
    {
        out.mainThrustAuthority01 = function;
        out.mainVectoringAuthority01 = function;
        out.manoeuvreAuthority01 = function;
    }
}

} // namespace game::simulation
