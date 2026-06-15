#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace game::navigation
{

struct GravityBody
{
    std::string id;

    glm::dvec3 centerMeters {0.0};

    double radiusMeters = 0.0;
    double gravitationalParameterM3s2 = 0.0;

    double atmosphereRadiusMeters = 0.0;
    double influenceRadiusMeters = 0.0;
};

struct GravityFieldSample
{
    glm::dvec3 accelerationMps2 {0.0};

    std::string primaryBodyId;

    double primaryDistanceMeters = 0.0;
    double primaryAltitudeMeters = 0.0;
    double primaryAccelerationMps2 = 0.0;

    bool insideInfluence = false;
    bool insideAtmosphere = false;
};

class GravityFieldSystem
{
public:
    static GravityFieldSample sample(
        const glm::dvec3& positionMeters,
        const std::vector<GravityBody>& bodies
    )
    {
        GravityFieldSample result;

        double bestAccel =
            0.0;

        for (const GravityBody& body : bodies)
        {
            if (body.gravitationalParameterM3s2 <= 0.0)
                continue;

            const glm::dvec3 toBody =
                body.centerMeters - positionMeters;

            const double distance =
                glm::length(toBody);

            if (distance <= 1.0)
                continue;

            if (body.influenceRadiusMeters > 0.0 &&
                distance > body.influenceRadiusMeters)
            {
                continue;
            }

            const glm::dvec3 dir =
                toBody / distance;

            const double accel =
                body.gravitationalParameterM3s2 /
                (distance * distance);

            result.accelerationMps2 +=
                dir * accel;

            if (accel > bestAccel)
            {
                bestAccel = accel;

                result.primaryBodyId =
                    body.id;

                result.primaryDistanceMeters =
                    distance;

                result.primaryAltitudeMeters =
                    distance - body.radiusMeters;

                result.primaryAccelerationMps2 =
                    accel;

                result.insideInfluence = true;

                result.insideAtmosphere =
                    body.atmosphereRadiusMeters > 0.0 &&
                    distance <= body.atmosphereRadiusMeters;
            }
        }

        return result;
    }
};

} // namespace game::navigation