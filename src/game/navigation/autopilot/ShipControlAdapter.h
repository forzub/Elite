#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/core/ShipParams.h"

namespace game::navigation::autopilot
{

// Converts navigation guidance into the SAME pilot controls used by a human.
//
// This adapter is deliberately above ship physics. It may press/hold the
// ordinary pitch/yaw/roll, +/- longitudinal and RCS controls, but it must not
// select engines, inject SYSTEM-frame acceleration, or bypass ShipController /
// DynamicMotionSystem.
//
// Assisted: +/- changes the ordinary persistent forward-speed setpoint.
// Newtonian: '+' is ordinary primary-main throttle; braking uses END.
// Precision capture: keypad RCS removes the final small residual velocity.
class ShipControlAdapter final
{
public:
    struct Request
    {
        LocalFlightControlLaw law = defaultLocalFlightControlLaw();

        glm::dvec3 desiredVelocityMapMps {0.0};
        glm::dvec3 desiredLinearAccelerationMapMps2 {0.0};
        glm::dvec3 desiredAngularAccelerationMapRadPerSec2 {0.0};

        glm::dvec3 actualVelocityMapMps {0.0};

        glm::dvec3 forwardMap {0.0, 0.0, -1.0};
        glm::dvec3 rightMap {1.0, 0.0, 0.0};
        glm::dvec3 upMap {0.0, 1.0, 0.0};

        // Persistent Assisted setpoint currently owned by the ship flight law.
        double currentAssistedTargetSpeedMps = 0.0;

        // Explicit authored command state. This is NOT inferred from measured
        // near-zero speed.
        bool stopRequested = false;

        double deltaSeconds = 0.0;
    };

    [[nodiscard]] static ShipControlState make(
        const Request& request,
        const ShipParams& params
    ) noexcept
    {
        ShipControlState out;

        const glm::dvec3 forward =
            normalizedOr(request.forwardMap, {0.0, 0.0, -1.0});
        const glm::dvec3 right =
            normalizedOr(request.rightMap, {1.0, 0.0, 0.0});
        const glm::dvec3 up =
            normalizedOr(request.upMap, {0.0, 1.0, 0.0});

        // ----- attitude: ordinary pitch/yaw/roll controls -----
        const double angularAuthority =
            game::ship::angularAccelerationLimitRadPerSec2(params);
        if (angularAuthority > 1.0e-12)
        {
            glm::dvec3 input(
                glm::dot(
                    request.desiredAngularAccelerationMapRadPerSec2,
                    right
                ) / angularAuthority,
                glm::dot(
                    request.desiredAngularAccelerationMapRadPerSec2,
                    up
                ) / angularAuthority,
                glm::dot(
                    request.desiredAngularAccelerationMapRadPerSec2,
                    forward
                ) / angularAuthority
            );

            const double magnitude = glm::length(input);
            if (std::isfinite(magnitude) && magnitude > 1.0)
                input /= magnitude;

            out.pitchInput = finiteClamp(input.x);
            out.yawInput = finiteClamp(input.y);
            out.rollInput = finiteClamp(input.z);
        }

        const double desiredSpeed =
            finiteLength(request.desiredVelocityMapMps);
        const double actualSpeed =
            finiteLength(request.actualVelocityMapMps);
        const double dt = std::max(
            0.0,
            std::isfinite(request.deltaSeconds)
                ? request.deltaSeconds
                : 0.0
        );

        constexpr double PrecisionStopEntrySpeedMps = 0.50;
        const bool precisionStop =
            request.stopRequested &&
            actualSpeed <= PrecisionStopEntrySpeedMps;

        // ----- precision STOP: ordinary keypad RCS -----
        if (precisionStop && dt > 1.0e-9)
        {
            const double rcsAuthority =
                game::ship::manoeuvreAccelerationLimitMps2(params);
            if (rcsAuthority > 1.0e-12)
            {
                glm::dvec3 wantedAcceleration =
                    (request.desiredVelocityMapMps -
                     request.actualVelocityMapMps) / dt;

                const double wantedMagnitude =
                    glm::length(wantedAcceleration);
                if (std::isfinite(wantedMagnitude) &&
                    wantedMagnitude > rcsAuthority)
                {
                    wantedAcceleration *=
                        rcsAuthority / wantedMagnitude;
                }

                out.forwardInput = finiteClamp(
                    glm::dot(wantedAcceleration, forward) /
                    rcsAuthority
                );
                out.strafeInput = finiteClamp(
                    glm::dot(wantedAcceleration, right) /
                    rcsAuthority
                );
                out.liftInput = finiteClamp(
                    glm::dot(wantedAcceleration, up) /
                    rcsAuthority
                );
            }
        }

        // ----- translation: same longitudinal control as the player -----
        if (request.law == LocalFlightControlLaw::Assisted)
        {
            const double maxSpeed =
                game::ship::controlledSpeedLimitMps(params);
            const double targetRateMps2 = std::max(
                static_cast<double>(
                    params.assistedMinimumTargetSpeedChangeRateMps2
                ),
                maxSpeed *
                    static_cast<double>(
                        params.assistedTargetSpeedChangeRateFractionPerSecond
                    )
            );

            if (dt > 1.0e-9 && targetRateMps2 > 1.0e-12)
            {
                const double target =
                    request.stopRequested ? 0.0 : desiredSpeed;
                const double error =
                    target - std::max(
                        0.0,
                        std::isfinite(
                            request.currentAssistedTargetSpeedMps
                        )
                            ? request.currentAssistedTargetSpeedMps
                            : 0.0
                    );
                out.targetSpeedRate = finiteClamp(
                    error / (targetRateMps2 * dt)
                );
            }
        }
        else
        {
            if (request.stopRequested && !precisionStop)
            {
                // Same END command available to the human pilot. ShipController
                // owns the flip/alignment and the real main bank owns braking.
                out.velocityAlignmentCommand =
                    VelocityAlignmentMode::BrakeToStop;
            }
            else if (!precisionStop)
            {
                const double aftPrimaryAuthority =
                    game::ship::forwardMainAccelerationLimitMps2(params);
                const double foreFallbackAuthority =
                    game::ship::reverseMainAccelerationLimitMps2(params);

                const bool aftPrimaryAvailable =
                    aftPrimaryAuthority > 1.0e-12;
                const double primaryAuthority =
                    aftPrimaryAvailable
                        ? aftPrimaryAuthority
                        : foreFallbackAuthority;
                const glm::dvec3 primaryThrustDirection =
                    aftPrimaryAvailable ? forward : -forward;

                if (primaryAuthority > 1.0e-12)
                {
                    const double alongPrimary =
                        glm::dot(
                            request.desiredLinearAccelerationMapMps2,
                            primaryThrustDirection
                        );
                    out.targetSpeedRate = static_cast<float>(
                        std::clamp(
                            std::isfinite(alongPrimary)
                                ? alongPrimary / primaryAuthority
                                : 0.0,
                            0.0,
                            1.0
                        )
                    );
                }
            }
        }

        // Hard invariant: autopilot output is pilot input, never the old
        // direct-navigation actuator seam.
        out.navigationAccelerationDemandValid = false;
        out.navigationVelocityTargetValid = false;
        out.navigationPrecisionTranslationOnly = false;
        return out;
    }

private:
    [[nodiscard]] static double finiteLength(
        const glm::dvec3& value
    ) noexcept
    {
        const double n = glm::length(value);
        return std::isfinite(n) ? n : 0.0;
    }

    [[nodiscard]] static glm::dvec3 normalizedOr(
        const glm::dvec3& value,
        const glm::dvec3& fallback
    ) noexcept
    {
        const double n = glm::length(value);
        return std::isfinite(n) && n > 1.0e-12
            ? value / n
            : fallback;
    }

    [[nodiscard]] static float finiteClamp(double value) noexcept
    {
        return static_cast<float>(
            std::clamp(
                std::isfinite(value) ? value : 0.0,
                -1.0,
                1.0
            )
        );
    }
};

} // namespace game::navigation::autopilot
