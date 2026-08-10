#include "src/game/presentation/StarSystemLabelPresentation.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace game::presentation
{
std::string buildGameSystemSkyLabel(
    std::string_view gameSystemName,
    std::string_view fallbackName,
    std::string_view fallbackId,
    double distanceLy
)
{
    const std::string_view displayName =
        !gameSystemName.empty()
            ? gameSystemName
            : (!fallbackName.empty() ? fallbackName : fallbackId);

    std::ostringstream out;
    out << displayName
        << "  "
        << std::fixed
        << std::setprecision(1)
        << std::max(0.0, distanceLy)
        << " ly";

    return out.str();
}
}
