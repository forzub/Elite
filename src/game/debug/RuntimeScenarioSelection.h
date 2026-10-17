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
    PropulsionMatrix,
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

    static RuntimeScenarioSelection parse(std::string_view value)
    {
        RuntimeScenarioSelection out;
        out.spec = std::string(value);

        if (value.empty())
            return out;

        if (value == "ship-control")
        {
            out.kind = RuntimeScenarioKind::ShipControl;
            return out;
        }

        if (value == "propulsion-matrix")
        {
            out.kind = RuntimeScenarioKind::PropulsionMatrix;
            return out;
        }

        constexpr std::string_view DamagePrefix = "ship-damage:";
        if (value.rfind(DamagePrefix, 0) == 0)
        {
            const auto payload = value.substr(DamagePrefix.size());
            if (payload.empty())
                return {};

            out.kind = RuntimeScenarioKind::ShipDamage;
            out.payload = std::string(payload);
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
            const auto payload =
                value.substr(NavPayloadPrefix.size());

            if (payload != "simple" &&
                payload != "assisted-high-speed" &&
                payload != "assisted-gameplay-parity" &&
                payload != "assisted-safe-course-obstacle")
            {
                return {};
            }

            out.kind = RuntimeScenarioKind::NavigationStress;
            out.payload = std::string(payload);
            return out;
        }

        return out;
    }

    static RuntimeScenarioSelection fromProcessEnvironment()
    {
        const char* raw = std::getenv("ELITE_DIAGNOSTIC_SCENARIO");
        return raw && *raw
            ? parse(raw)
            : RuntimeScenarioSelection {};
    }
};

} // namespace game::debug
