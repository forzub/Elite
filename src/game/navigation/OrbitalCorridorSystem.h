#pragma once

#include <cmath>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/GravityFieldSystem.h"

namespace game::navigation
{

enum class OrbitalCorridorStateType
{
    None,
    BelowDanger,
    InCorridor,
    AboveCorridor,
    EscapeRegion
};

struct OrbitalCorridor
{
    std::string id;
    std::string parentBodyId;
    std::string hubId;

    double targetAltitudeMeters = 0.0;
    double halfWidthMeters = 50000.0;

    double dangerBelowMeters = 50000.0;
    double escapeAboveMeters = 50000.0;
};

struct OrbitalCorridorSample
{
    OrbitalCorridorStateType state =
        OrbitalCorridorStateType::None;

    std::string corridorId;
    std::string parentBodyId;
    std::string hubId;

    double altitudeMeters = 0.0;
    double altitudeErrorMeters = 0.0;

    double targetCircularSpeedMps = 0.0;
    double tangentialSpeedMps = 0.0;
    double radialSpeedMps = 0.0;
    double speedErrorMps = 0.0;

    bool valid = false;
};

class OrbitalCorridorSystem
{
public:
    static OrbitalCorridorSample classify(
        const glm::dvec3& positionMeters,
        const glm::dvec3& velocityMps,
        const GravityBody& body,
        const OrbitalCorridor& corridor
    )
    {
        OrbitalCorridorSample result;

        result.parentBodyId =
            body.id;

        result.corridorId =
            corridor.id;

        result.hubId =
            corridor.hubId;

        const glm::dvec3 fromBody =
            positionMeters - body.centerMeters;

        const double distance =
            glm::length(fromBody);

        if (distance <= 1.0 ||
            body.radiusMeters <= 0.0 ||
            body.gravitationalParameterM3s2 <= 0.0)
        {
            return result;
        }

        const glm::dvec3 radial =
            fromBody / distance;

        result.altitudeMeters =
            distance - body.radiusMeters;

        result.altitudeErrorMeters =
            result.altitudeMeters -
            corridor.targetAltitudeMeters;

        const double orbitRadius =
            body.radiusMeters +
            corridor.targetAltitudeMeters;

        if (orbitRadius > 1.0)
        {
            result.targetCircularSpeedMps =
                std::sqrt(
                    body.gravitationalParameterM3s2 /
                    orbitRadius
                );
        }

        result.radialSpeedMps =
            glm::dot(velocityMps, radial);

        const glm::dvec3 tangentialVelocity =
            velocityMps - radial * result.radialSpeedMps;

        result.tangentialSpeedMps =
            glm::length(tangentialVelocity);

        result.speedErrorMps =
            result.tangentialSpeedMps -
            result.targetCircularSpeedMps;

        const double absAltError =
            std::abs(result.altitudeErrorMeters);

        if (absAltError <= corridor.halfWidthMeters)
        {
            result.state =
                OrbitalCorridorStateType::InCorridor;
        }
        else if (result.altitudeErrorMeters < -corridor.dangerBelowMeters)
        {
            result.state =
                OrbitalCorridorStateType::BelowDanger;
        }
        else if (result.altitudeErrorMeters > corridor.escapeAboveMeters)
        {
            result.state =
                OrbitalCorridorStateType::EscapeRegion;
        }
        else
        {
            result.state =
                OrbitalCorridorStateType::AboveCorridor;
        }

        result.valid = true;
        return result;
    }
};

} // namespace game::navigation