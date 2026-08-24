#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/GravityFieldSystem.h"
#include "src/world/navigation/NavigationObstacle.h"

namespace game::navigation
{

enum class NavigationKnowledgeSource : std::uint8_t
{
    Unknown = 0,
    AuthoritativeWorld,
    CelestialEphemeris,
    OfficialLaneCatalog,
    TrafficControl,
    NavigationBeacon,
    Transponder,
    Radar,
    Estimated
};

enum class NavigationLaneStatus : std::uint8_t
{
    Open = 0,
    Restricted,
    Closed
};

enum class NavigationLaneDirection : std::uint8_t
{
    Bidirectional = 0,
    ForwardOnly,
    ReverseOnly
};

struct NavigationLane
{
    std::string id;
    int systemId = -1;
    std::vector<glm::dvec3> centerlineMeters;

    double corridorRadiusMeters = 0.0;
    double preferredSpeedMps = 0.0;
    double maxSpeedMps = 0.0;
    double nominalSeparationMeters = 0.0;

    NavigationLaneStatus status = NavigationLaneStatus::Open;
    NavigationLaneDirection direction = NavigationLaneDirection::Bidirectional;

    bool beaconServed = false;
    double positionUncertaintyMeters = 0.0;

    // Multiplicative planning preference. Values below 1 make a lane cheaper
    // than an equally long free-space segment; this is not a physics rule.
    double planningCostMultiplier = 0.75;
};

struct NavigationObstacleState
{
    int systemId = -1;

    double epochUniverseTimeSeconds = 0.0;
    world::navigation::NavigationObstacle geometry;
    glm::dvec3 velocityMps {0.0};
    glm::dvec3 accelerationMps2 {0.0};

    double positionUncertaintyMeters = 0.0;
    double velocityUncertaintyMps = 0.0;

    double validFromUniverseTimeSeconds = 0.0;
    double validUntilUniverseTimeSeconds = 0.0;

    NavigationKnowledgeSource source = NavigationKnowledgeSource::Unknown;
};
struct KnownTrafficSample
{
    double universeTimeSeconds = 0.0;
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};
};

struct KnownTrafficIntent
{
    std::string id;
    int systemId = -1;
    std::string displayName;

    std::vector<KnownTrafficSample> samples;

    double physicalRadiusMeters = 0.0;
    double requiredSeparationMeters = 0.0;
    double positionUncertaintyMeters = 0.0;
    double timingUncertaintySeconds = 0.0;

    NavigationKnowledgeSource source =
        NavigationKnowledgeSource::TrafficControl;
};

struct RestrictedNavigationVolume
{
    std::string id;
    int systemId = -1;
    glm::dvec3 centerMeters {0.0};
    double radiusMeters = 0.0;
    double clearanceMeters = 0.0;

    double validFromUniverseTimeSeconds = 0.0;
    double validUntilUniverseTimeSeconds = 0.0;
};

struct NavigationPlanningQuery
{
    int systemId = -1;
    glm::dvec3 regionCenterMeters {0.0};
    double regionRadiusMeters = 0.0;

    double startUniverseTimeSeconds = 0.0;
    double endUniverseTimeSeconds = 0.0;

    double shipSafetyRadiusMeters = 0.0;
};

/*
    Immutable-by-convention input product for route/local planners.

    The server, radar, beacons and transponders are data producers. They do not
    call TrajectoryPredictor directly. A caller builds one coherent snapshot,
    then route/predict/safety code works without network latency or hidden IO.
*/
struct NavigationPlanningSnapshot
{
    int systemId = -1;
    double generatedAtUniverseTimeSeconds = 0.0;
    double validUntilUniverseTimeSeconds = 0.0;

    std::vector<GravityBody> gravityBodies;
    std::vector<NavigationLane> officialLanes;
    std::vector<NavigationObstacleState> obstacles;
    std::vector<KnownTrafficIntent> scheduledTraffic;
    std::vector<RestrictedNavigationVolume> restrictedVolumes;
};

/*
    Minimal fusion seam for later server/sensor integration.

    A more precise local observation may refine position/velocity uncertainty,
    but it cannot shrink authoritative physical size or required clearance.
*/
class NavigationPlanningSnapshotBuilder
{
public:
    explicit NavigationPlanningSnapshotBuilder(
        NavigationPlanningSnapshot base = {}
    )
        : m_snapshot(std::move(base))
    {
    }

    void mergeObstacleObservation(const NavigationObstacleState& observation)
    {
        if (observation.geometry.id.empty())
            return;

        for (NavigationObstacleState& existing : m_snapshot.obstacles)
        {
            if (existing.geometry.id != observation.geometry.id)
                continue;

            const bool observationMorePrecise =
                existing.positionUncertaintyMeters <= 0.0 ||
                (observation.positionUncertaintyMeters >= 0.0 &&
                 observation.positionUncertaintyMeters <
                     existing.positionUncertaintyMeters);

            if (observationMorePrecise)
            {
                existing.epochUniverseTimeSeconds =
                    observation.epochUniverseTimeSeconds;
                existing.geometry.centerMeters = observation.geometry.centerMeters;
                existing.geometry.localToWorldBasis =
                    observation.geometry.localToWorldBasis;
                existing.velocityMps = observation.velocityMps;
                existing.accelerationMps2 = observation.accelerationMps2;
                existing.positionUncertaintyMeters =
                    observation.positionUncertaintyMeters;
                existing.velocityUncertaintyMps =
                    observation.velocityUncertaintyMps;
                existing.source = observation.source;
            }

            // Sensor fusion may refine pose but must never shrink the
            // authoritative physical envelope. If shape classifications disagree,
            // retain the larger conservative envelope as a sphere.
            const double existingRadius =
                existing.geometry.conservativeRadiusMeters();
            const double observationRadius =
                observation.geometry.conservativeRadiusMeters();
            if (existing.geometry.shape == observation.geometry.shape)
            {
                if (existing.geometry.shape ==
                    world::navigation::NavigationObstacleShape::Box)
                {
                    existing.geometry.halfExtentsMeters = glm::max(
                        existing.geometry.halfExtentsMeters,
                        observation.geometry.halfExtentsMeters
                    );
                }
                else if (existing.geometry.shape ==
                    world::navigation::NavigationObstacleShape::Capsule)
                {
                    existing.geometry.radiusMeters = std::max(
                        existing.geometry.radiusMeters,
                        observation.geometry.radiusMeters
                    );
                    existing.geometry.capsuleHalfLengthMeters = std::max(
                        existing.geometry.capsuleHalfLengthMeters,
                        observation.geometry.capsuleHalfLengthMeters
                    );
                }
                else
                {
                    existing.geometry.radiusMeters = std::max(
                        existing.geometry.radiusMeters,
                        observation.geometry.radiusMeters
                    );
                }
            }
            else if (observationRadius > existingRadius)
            {
                existing.geometry.shape =
                    world::navigation::NavigationObstacleShape::Sphere;
                existing.geometry.radiusMeters = observationRadius;
            }
            existing.geometry.requiredClearanceMeters = std::max(
                existing.geometry.requiredClearanceMeters,
                observation.geometry.requiredClearanceMeters
            );
            return;
        }

        m_snapshot.obstacles.push_back(observation);
    }

    NavigationPlanningSnapshot build() &&
    {
        return std::move(m_snapshot);
    }

private:
    NavigationPlanningSnapshot m_snapshot;
};

} // namespace game::navigation
