#pragma once

#include <cstddef>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/planner/RoutePlannerApi.h"

namespace game::navigation::planner
{

// Immutable tail of a route whose expensive downstream geometry no longer
// depends on the live ship state. Only the entry connector is rebound while
// the craft continues ballistic flight.
struct LateBoundRouteBackbone
{
    bool valid = false;
    std::string failure;

    RoutePlan tail;
    glm::dvec3 anchorMeters {0.0};
    glm::dvec3 anchorForward {0.0, 0.0, -1.0};
    double handoffSpeedMps = 0.0;
};

enum class BallisticPrefixDisposition : std::uint8_t
{
    Invalid = 0,
    BeforeStart,
    OnPrefix,
    PastPrefix,
    OffAxis
};

struct BallisticPrefixStatus
{
    BallisticPrefixDisposition disposition =
        BallisticPrefixDisposition::Invalid;
    double alongMeters = 0.0;
    double crossTrackMeters = 0.0;
    double prefixLengthMeters = 0.0;
    double remainingMeters = 0.0;
};

class RouteEntryBinder final
{
public:
    // preferredAnchor is used for semantic traffic routes (normally the outer
    // boundary of the first mandatory BLUE block). If hasPreferredAnchor is
    // false, the binder preserves the route's final terminal-approach curve as
    // the immutable tail and rebinds everything before it.
    [[nodiscard]] static LateBoundRouteBackbone extractBackbone(
        const RoutePlan& source,
        bool hasPreferredAnchor,
        const glm::dvec3& preferredAnchor,
        const glm::dvec3& preferredForward
    );

    [[nodiscard]] static RoutePlan bind(
        const RoutePlanRequest& liveRequest,
        const LateBoundRouteBackbone& backbone
    );

    // Spatial handoff: wall-clock planner duration is deliberately absent.
    // A computed route remains usable while the ballistic craft is still on
    // its authored first straight.
    [[nodiscard]] static BallisticPrefixStatus inspectBallisticPrefix(
        const RoutePlan& plan,
        const glm::dvec3& livePositionMeters,
        double allowedCrossTrackMeters
    ) noexcept;
};

} // namespace game::navigation::planner
