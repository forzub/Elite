#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <utility>

#include "src/game/navigation/NavigationModuleState.h"

namespace game::navigation
{

enum class GuidanceSource : std::uint8_t
{
    Unknown = 0,
    RouteSolver,
    LocalPlanner,
    DockingComputer,
    StationTrafficControl,
    Mission,
    Fleet,
    EmergencyControl
};

enum class GuidancePurpose : std::uint8_t
{
    Transit = 0,
    Approach,
    Docking,
    Landing,
    CargoApproach,
    ObstacleBypass,
    AttackRun,
    FormationJoin,
    Departure,
    EmergencyEscape
};

/*
    One time-aware cross-section of a visual/operational flight corridor.

    Position is system-local full metres, consistent with TrajectoryPredictor.
    Orientation always describes the visible corridor frame. For docking/other
    6-DOF terminal guidance, requiredVehiclePose=true upgrades that orientation
    to an explicit desired hull attitude: +X right, +Y top, -Z nose. No control
    is implied; the corridor remains advisory until a follower is added later.
*/
struct GuidanceFrame
{
    double universeTimeSeconds = 0.0;
    glm::dvec3 centerMeters {0.0};
    glm::dquat orientation {1.0, 0.0, 0.0, 0.0};

    double widthMeters = 0.0;
    double heightMeters = 0.0;

    double recommendedSpeedMps = 0.0;
    double maxClosureRateMps = 0.0;

    double lateralToleranceMeters = 0.0;
    double verticalToleranceMeters = 0.0;

    bool requiredVehiclePose = false;
};

struct GuidanceCorridor
{
    std::string id;
    int systemId = -1;

    GuidanceSource source = GuidanceSource::Unknown;
    GuidancePurpose purpose = GuidancePurpose::Transit;

    double generatedAtUniverseTimeSeconds = 0.0;
    double validUntilUniverseTimeSeconds = 0.0;

    // 0..1 expresses trust in the supplied path, not HUD opacity.
    double confidence = 1.0;
    int priority = 0;
    bool advisoryOnly = true;

    // Manual docking tunnels are rebuilt at the current guidance epoch. Their
    // frames are spatial/equidistant gates rather than future-time samples.
    bool spatialManualTunnel = false;

    // EmergencyEscape is still a corridor, not an autopilot command.  The
    // presentation layer may flash a warning while this flag is active.
    bool noSafePrimarySolution = false;

    // For terminal manoeuvres the accepted physical prediction and the
    // required endpoint stay separate. Render/debug code can therefore show
    // the actual raw end and the requested docking point without moving either.
    bool hasTerminalTarget = false;
    glm::dvec3 terminalTargetMeters {0.0};
    double terminalPositionErrorMeters = 0.0;

    std::vector<GuidanceFrame> frames;

    bool validAt(double universeTimeSeconds) const noexcept
    {
        if (id.empty() || systemId < 0 || frames.empty())
            return false;

        if (validUntilUniverseTimeSeconds > generatedAtUniverseTimeSeconds &&
            universeTimeSeconds > validUntilUniverseTimeSeconds)
        {
            return false;
        }

        return true;
    }
};

class NavigationGuidanceState
{
public:
    void publish(GuidanceCorridor corridor)
    {
        if (corridor.id.empty())
            return;

        for (GuidanceCorridor& existing : m_corridors)
        {
            if (existing.id == corridor.id)
            {
                existing = std::move(corridor);
                return;
            }
        }

        m_corridors.push_back(std::move(corridor));
    }

    bool erase(const std::string& id)
    {
        const auto oldSize = m_corridors.size();
        m_corridors.erase(
            std::remove_if(
                m_corridors.begin(),
                m_corridors.end(),
                [&](const GuidanceCorridor& corridor)
                {
                    return corridor.id == id;
                }
            ),
            m_corridors.end()
        );
        return m_corridors.size() != oldSize;
    }

    void clear() noexcept
    {
        m_corridors.clear();
    }

    const std::vector<GuidanceCorridor>& corridors() const noexcept
    {
        return m_corridors;
    }

    static bool sourceEnabled(
        GuidanceSource source,
        const NavigationModuleState& modules
    ) noexcept
    {
        switch (source)
        {
            case GuidanceSource::RouteSolver:
                return modules.enabled(NavigationModuleId::RoutePlanning);
            case GuidanceSource::LocalPlanner:
            case GuidanceSource::DockingComputer:
                return modules.enabled(NavigationModuleId::LocalGuidance);
            case GuidanceSource::StationTrafficControl:
            case GuidanceSource::Mission:
            case GuidanceSource::Fleet:
                return modules.enabled(NavigationModuleId::ServerGuidance);
            case GuidanceSource::EmergencyControl:
                return modules.enabled(NavigationModuleId::LocalGuidance);
            case GuidanceSource::Unknown:
            default:
                return true;
        }
    }

    const GuidanceCorridor* active(
        int systemId,
        double universeTimeSeconds,
        const NavigationModuleState* modules = nullptr
    ) const noexcept
    {
        return activeFiltered(
            systemId,
            universeTimeSeconds,
            modules,
            [](const GuidanceCorridor&) { return true; }
        );
    }

    // Map trajectory and cockpit manual guidance are different products.
    // The map must keep showing the accepted time-parameterized trajectory
    // while a higher-priority spatial tunnel is regenerated from live ship
    // and dock poses for the HUD.
    const GuidanceCorridor* activePredictive(
        int systemId,
        double universeTimeSeconds,
        const NavigationModuleState* modules = nullptr
    ) const noexcept
    {
        return activeFiltered(
            systemId,
            universeTimeSeconds,
            modules,
            [](const GuidanceCorridor& corridor)
            {
                return !corridor.spatialManualTunnel;
            }
        );
    }

    const GuidanceCorridor* activeSpatialManualTunnel(
        int systemId,
        double universeTimeSeconds,
        const NavigationModuleState* modules = nullptr
    ) const noexcept
    {
        return activeFiltered(
            systemId,
            universeTimeSeconds,
            modules,
            [](const GuidanceCorridor& corridor)
            {
                return corridor.spatialManualTunnel;
            }
        );
    }

    void pruneExpired(double universeTimeSeconds)
    {
        m_corridors.erase(
            std::remove_if(
                m_corridors.begin(),
                m_corridors.end(),
                [&](const GuidanceCorridor& corridor)
                {
                    return corridor.validUntilUniverseTimeSeconds >
                               corridor.generatedAtUniverseTimeSeconds &&
                           universeTimeSeconds >
                               corridor.validUntilUniverseTimeSeconds;
                }
            ),
            m_corridors.end()
        );
    }

private:
    template <typename Predicate>
    const GuidanceCorridor* activeFiltered(
        int systemId,
        double universeTimeSeconds,
        const NavigationModuleState* modules,
        Predicate predicate
    ) const noexcept
    {
        const GuidanceCorridor* best = nullptr;
        for (const GuidanceCorridor& corridor : m_corridors)
        {
            if (corridor.systemId != systemId ||
                !corridor.validAt(universeTimeSeconds) ||
                !predicate(corridor) ||
                (modules && !sourceEnabled(corridor.source, *modules)))
            {
                continue;
            }

            if (!best || corridor.priority > best->priority ||
                (corridor.priority == best->priority &&
                 corridor.generatedAtUniverseTimeSeconds >
                     best->generatedAtUniverseTimeSeconds))
            {
                best = &corridor;
            }
        }
        return best;
    }

    std::vector<GuidanceCorridor> m_corridors;
};

} // namespace game::navigation
