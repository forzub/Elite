#pragma once

#include <cstdint>

namespace game::presentation
{

enum class PresentationPolicy : std::uint8_t
{
    Analytic = 0,
    SnapshotInterpolated,
    LocalPredicted
};

enum class PresentationRole : std::uint8_t
{
    LocalControlled = 0,
    RemoteObserved,
    DiagnosticReference
};

} // namespace game::presentation
