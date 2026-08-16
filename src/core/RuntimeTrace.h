#pragma once

#include <cstdlib>
#include <string_view>

namespace core
{

inline bool runtimeTraceEnabled() noexcept
{
    static const bool enabled = [] {
        const char* value = std::getenv("ELITE_TRACE_RUNTIME");
        if (!value)
            return false;

        const std::string_view text(value);
        return !text.empty() &&
            text != "0" &&
            text != "false" &&
            text != "FALSE" &&
            text != "off" &&
            text != "OFF";
    }();
    return enabled;
}

} // namespace core
