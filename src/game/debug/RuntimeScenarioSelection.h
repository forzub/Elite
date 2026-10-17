#pragma once

#include <cstdlib>
#include <string>
#include <string_view>

namespace game::debug
{

enum class RuntimeScenarioKind
{
    None = 0,
    ShipControl,
    ShipDamage,
    NavigationStress
};

struct RuntimeScenarioSelection
{
    RuntimeScenarioKind kind = RuntimeScenarioKind::None;
    std::string spec;
    std::string payload;

    bool active() const noexcept
    {
        return kind != RuntimeScenarioKind::None;
    }

    static RuntimeScenarioSelection fromProcessEnvironment()
    {
        RuntimeScenarioSelection out;

        const char* raw = std::getenv("ELITE_DIAGNOSTIC_SCENARIO");
        if (!raw || !*raw)
            return out;

        out.spec = raw;
        const std::string_view value(out.spec);

        if (value == "ship-control")
        {
            out.kind = RuntimeScenarioKind::ShipControl;
            return out;
        }

        constexpr std::string_view DamagePrefix = "ship-damage:";
        if (value.rfind(DamagePrefix, 0) == 0)
        {
            out.kind = RuntimeScenarioKind::ShipDamage;
            out.payload = std::string(value.substr(DamagePrefix.size()));
            return out;
        }

        constexpr std::string_view NavPrefix = "nav-stress";
        if (value == NavPrefix)
        {
            out.kind = RuntimeScenarioKind::NavigationStress;
            return out;
        }

        constexpr std::string_view NavPayloadPrefix = "nav-stress:";
        if (value.rfind(NavPayloadPrefix, 0) == 0)
        {
            out.kind = RuntimeScenarioKind::NavigationStress;
            out.payload = std::string(value.substr(NavPayloadPrefix.size()));
            return out;
        }

        return out;
    }
};

} // namespace game::debug
