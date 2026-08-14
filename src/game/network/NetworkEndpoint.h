#pragma once

#include <cstdint>
#include <string>

namespace game::network
{
struct NetworkEndpoint
{
    std::string host;
    std::uint16_t port = 0;
};

inline bool parseNetworkEndpoint(
    const std::string& text,
    NetworkEndpoint& out,
    std::string* error = nullptr)
{
    const auto colon = text.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= text.size())
    {
        if (error)
            *error = "endpoint must be HOST:PORT";
        return false;
    }

    const std::string host = text.substr(0, colon);
    const std::string portText = text.substr(colon + 1);

    std::uint64_t portValue = 0;
    for (const char c : portText)
    {
        if (c < '0' || c > '9')
        {
            if (error)
                *error = "endpoint port must be numeric";
            return false;
        }
        portValue = portValue * 10u + static_cast<std::uint64_t>(c - '0');
        if (portValue > 65535u)
        {
            if (error)
                *error = "endpoint port is out of range";
            return false;
        }
    }

    if (portValue == 0u)
    {
        if (error)
            *error = "endpoint port must be non-zero";
        return false;
    }

    out.host = host;
    out.port = static_cast<std::uint16_t>(portValue);
    return true;
}
}
