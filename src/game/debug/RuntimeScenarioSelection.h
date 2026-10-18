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
