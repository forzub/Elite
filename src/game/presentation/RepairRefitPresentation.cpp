#include "src/game/presentation/RepairRefitPresentation.h"

#include <algorithm>
#include <array>

#include "src/game/catalog/ships/ShipDesignCatalog.h"
#include "src/world/modules/ObjectModuleState.h"

namespace game::presentation
{
namespace
{
const char* stateName(std::uint8_t s)
{
    using S = world::modules::ObjectModuleRuntimeState;
    switch (static_cast<S>(s))
    {
        case S::Attached: return "attached";
        case S::Disabled: return "disabled";
        case S::Destroyed: return "destroyed";
        case S::Detached: return "detached";
        case S::Hanging: return "hanging";
    }
    return "unknown";
}

constexpr std::array<std::pair<const char*, const char*>, 15> Groups {{
    {"propulsion", "Главная тяга"},
    {"propulsion_reactors", "Двигательные реакторы"},
    {"rcs", "Маневровые (RCS)"},
    {"service_power", "Служебное питание"},
    {"cooling", "Охлаждение"},
    {"tanks", "Баки"},
    {"sensors", "Радар и связь"},
    {"computer", "Бортовой компьютер"},
    {"life", "Жизнеобеспечение и спасение"},
    {"jump", "Прыжок"},
    {"weapons", "Вооружение"},
    {"payload", "Груз и пассажиры"},
    {"docks", "Стыковка и ангары"},
    {"structure", "Конструкция"},
    {"other", "Прочее"},
}};
} // namespace

std::string refitGroupOf(const std::string& category, const std::string& subsystemId)
{
    const std::string& c = category.empty() ? subsystemId : category;
    if (c == "drive" || c.find("engine") != std::string::npos || c.find("propulsion.main") != std::string::npos)
        return "propulsion";
    if (c == "reactor" || c.find("propulsion.reactor") != std::string::npos)
        return "propulsion_reactors";
    if (c == "rcs" || c.find("rcs") != std::string::npos)
        return "rcs";
    if (c == "service_reactor" || c.find("power") != std::string::npos)
        return "service_power";
    if (c == "coolant_loop" || c == "radiator" || c.find("cool") != std::string::npos || c.find("radiator") != std::string::npos)
        return "cooling";
    if (c == "tank")
        return "tanks";
    if (c == "radar" || c == "comm" || c == "transponder" || c == "ecm" || c == "sensor_mast" || c.find("sensor") != std::string::npos)
        return "sensors";
    if (c == "computer")
        return "computer";
    if (c == "life_support" || c == "escape_capsule" || c == "lifeboat" || c == "medbay" || c.find("cockpit") != std::string::npos)
        return "life";
    if (c == "jump_core")
        return "jump";
    if (c == "weapon")
        return "weapons";
    if (c == "container_rack" || c == "train_module" || c == "passenger_module" || c == "cryo_tank" || c == "bulk_hold" ||
        c.find("cargo") != std::string::npos)
        return "payload";
    if (c == "docking_port" || c == "hangar" || c == "boat_bay" || c == "drone_bay" || c == "tow_rig")
        return "docks";
    if (c == "hull" || c == "armour" || c == "heat_shield" || c.find("frame") != std::string::npos || c.find("hull") != std::string::npos ||
        c.find("structure") != std::string::npos)
        return "structure";
    return "other";
}

const char* refitGroupLabelRu(const std::string& key)
{
    for (const auto& [k, label] : Groups)
        if (key == k)
            return label;
    return "Прочее";
}

RepairRefitPresentation buildRepairRefitPresentation(
    const std::vector<RefitShipInput>& ownedShips,
    EntityId currentShip,
    const std::string& selectedGroup,
    const std::string& selectedModule,
    std::uint32_t seenApplySerial)
{
    RepairRefitPresentation out;
    const RefitShipInput* current = nullptr;
    for (std::size_t i = 0; i < ownedShips.size(); ++i)
    {
        const auto& s = ownedShips[i];
        if (!s.descriptor)
            continue;
        RefitShipTab tab;
        tab.id = s.id;
        tab.model = s.descriptor->identity.shipName;
        tab.name = s.refit && !s.refit->shipName.empty() ? s.refit->shipName : tab.model;
        tab.registration = s.refit ? s.refit->registrationId : std::string();
        if (s.id.value == currentShip.value)
        {
            out.currentShip = static_cast<int>(out.ships.size());
            current = &s;
        }
        out.ships.push_back(std::move(tab));
    }
    if (!current && !ownedShips.empty() && ownedShips.front().descriptor)
    {
        current = &ownedShips.front();
        out.currentShip = 0;
    }
    if (!current)
        return out;

    const auto& d = *current->descriptor;
    out.valid = true;
    out.modelName = d.identity.shipName;
    out.passportId = d.catalog.passportId;
    out.envelopeMeters = {d.logicalDimensionsValue.width, d.logicalDimensionsValue.height, d.logicalDimensionsValue.length};

    // every installed module with its replicated runtime state
    const auto stateOf = [&](const std::string& id) -> const game::simulation::ObjectModuleSnapshot* {
        if (!current->modules)
            return nullptr;
        for (const auto& m : *current->modules)
            if (m.moduleId == id)
                return &m;
        return nullptr;
    };
    for (const auto& m : d.modules)
    {
        RefitModuleView v;
        v.moduleId = m.moduleId;
        v.parentModuleId = m.parentModuleId;
        const auto facts = game::ship::refit::moduleFacts(d, m.moduleId);
        v.category = facts.category.empty() ? m.subsystemId : facts.category;
        v.group = refitGroupOf(m.subsystemId == "propulsion.rcs" ? std::string("rcs") : facts.category, m.subsystemId);
        v.label = facts.productName.empty() ? m.moduleId : facts.productName;
        v.critical = m.isCritical;
        v.hasPlacement = m.placement.valid;
        v.centerMeters = m.placement.centerMeters;
        v.halfSizeMeters = m.placement.halfSizeMeters;
        if (const auto* st = stateOf(m.moduleId))
        {
            v.state = stateName(st->state);
            v.health01 = m.maxHealth > 0.0f ? std::clamp(st->health / m.maxHealth, 0.0f, 1.0f) : 0.0;
            const std::array<std::pair<const char*, float>, 6> causes {{
                {"лазер", st->laserDamage01}, {"удар", st->impactDamage01}, {"перегрев", st->thermalDamage01},
                {"радиация", st->radiationDamage01}, {"износ", st->wearDamage01}, {"внутренний отказ", st->internalFailureDamage01}}};
            for (const auto& [name, value] : causes)
            {
                v.damage01 += value;
                if (value > 1e-4f)
                    v.causes.emplace_back(name, value);
            }
            v.damage01 = std::clamp(v.damage01, 0.0, 1.0);
            v.wear01 = st->wearDamage01;
        }
        else
            v.state = "unknown";
        out.modules.push_back(std::move(v));
    }

    // groups (only those the ship has)
    for (const auto& [key, label] : Groups)
    {
        RefitGroupView g;
        g.key = key;
        g.labelRu = label;
        for (const auto& m : out.modules)
            if (m.group == key)
            {
                ++g.modules;
                g.damaged += m.damaged() ? 1 : 0;
                g.failed += m.failed() ? 1 : 0;
            }
        if (g.modules == 0)
            continue;
        g.selected = g.key == selectedGroup;
        out.groups.push_back(std::move(g));
    }

    // selection: the asked module, else the worst module of the group, else the worst of the ship
    const auto worse = [](const RefitModuleView& a, const RefitModuleView& b) {
        const double sa = (a.failed() ? 2.0 : 0.0) + (1.0 - a.health01) + a.damage01;
        const double sb = (b.failed() ? 2.0 : 0.0) + (1.0 - b.health01) + b.damage01;
        return sa > sb;
    };
    for (std::size_t i = 0; i < out.modules.size(); ++i)
        if (out.modules[i].moduleId == selectedModule)
            out.selectedModule = static_cast<int>(i);
    if (out.selectedModule < 0)
    {
        int best = -1;
        for (std::size_t i = 0; i < out.modules.size(); ++i)
        {
            if (!selectedGroup.empty() && out.modules[i].group != selectedGroup)
                continue;
            if (best < 0 || worse(out.modules[i], out.modules[static_cast<std::size_t>(best)]))
                best = static_cast<int>(i);
        }
        out.selectedModule = best;
    }
    if (out.selectedModule >= 0)
    {
        auto& sel = out.modules[static_cast<std::size_t>(out.selectedModule)];
        sel.selected = true;
        out.facts = game::ship::refit::moduleFacts(d, sel.moduleId);
        out.replacements = game::ship::refit::compatibleReplacements(d, sel.moduleId);
        if (current->refit && current->refit->previewValid && current->refit->preview.operation.moduleId == sel.moduleId)
        {
            out.previewForSelection = true;
            out.preview = current->refit->preview;
        }
    }
    if (current->refit)
    {
        out.applyResult = current->refit->applyResult;
        out.applyResultFresh = current->refit->applySerial != 0 && current->refit->applySerial != seenApplySerial;
    }
    return out;
}
} // namespace game::presentation
