#pragma once

#include <string>
#include <string_view>

namespace game::presentation
{
std::string buildGameSystemSkyLabel(
    std::string_view gameSystemName,
    std::string_view fallbackName,
    std::string_view fallbackId,
    double distanceLy
);
}
