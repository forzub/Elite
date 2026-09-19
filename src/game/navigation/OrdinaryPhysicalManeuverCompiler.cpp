#include "OrdinaryPhysicalManeuverCompiler.h"

#include <algorithm>
#include <cmath>

namespace game::navigation
{
namespace
{

using Compiler = OrdinaryPhysicalManeuverCompiler;
using Candidate = OrdinaryPhysicalManeuverCandidate;

constexpr double kEpsilon = 1.0e-9;
constexpr double kAngleEpsilon = 1.0e-6;
constexpr double kMinimumPrimitiveSeconds = 0.05;

bool finite(double v) noexcept
{
    return std::isfinite(v);
}

bool finite(const glm::dvec3& v) noexcept
{
    return finite(v.x) && finite(v.y) && finite(v.z);
}

glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double n2 = glm::dot(value, value);
    if (!finite(n2) || n2 <= kEpsilon * kEpsilon)
        return fallback;
    return value / std::sqrt(n2);
}

bool orthonormalBasis(
    const glm::dvec3& forwardIn,
    const glm::dvec3& rightIn,
    const glm::dvec3& upIn,
    glm::dvec3& forward,
    glm::dvec3& right,
    glm::dvec3& up
) noexcept
{
    forward = normalizedOr(forwardIn, glm::dvec3(0.0));
    if (glm::dot(forward, forward) <= kEpsilon)
        return false;

    right =
        rightIn - forward * glm::dot(rightIn, forward);
    right = normalizedOr(right, glm::dvec3(0.0));

    if (glm::dot(right, right) <= kEpsilon)
    {
        const glm::dvec3 upSeed =
            upIn - forward * glm::dot(upIn, forward);
        const glm::dvec3 normalizedUp =
            normalizedOr(upSeed, glm::dvec3(0.0));
        if (glm::dot(normalizedUp, normalizedUp) <= kEpsilon)
            return false;

        right = normalizedOr(
            glm::cross(normalizedUp, forward),
            glm::dvec3(0.0)
        );
    }

    if (glm::dot(right, right) <= kEpsilon)
        return false;

    up = normalizedOr(
        glm::cross(forward, right),
        glm::dvec3(0.0)
    );
    if (glm::dot(up, up) <= kEpsilon)
        return false;

    right = normalizedOr(
        glm::cross(up, forward),
        right
    );
    return true;
}

glm::dvec3 rotateAroundAxis(
    const glm::dvec3& value,
    const glm::dvec3& axis,
    double angle
) noexcept
{
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    return
        value * c +
        glm::cross(axis, value) * s +
        axis * glm::dot(axis, value) * (1.0 - c);
}

double smoothQuintic(double s) noexcept
{
    const double s2 = s * s;
    const double s3 = s2 * s;
    const double s4 = s3 * s;
    const double s5 = s4 * s;
    return 10.0 * s3 - 15.0 * s4 + 6.0 * s5;
}

double smoothQuinticD1(double s) noexcept
{
    const double s2 = s * s;
    const double s3 = s2 * s;
    const double s4 = s3 * s;
    return 30.0 * s2 - 60.0 * s3 + 30.0 * s4;
}

double smoothQuinticD2(double s) noexcept
{
    const double s2 = s * s;
    const double s3 = s2 * s;
    return 60.0 * s - 180.0 * s2 + 120.0 * s3;
}

bool validQuery(
    const Compiler::Query& q
) noexcept
{
    const auto& c = q.capability;
    return
        finite(q.state.positionMapMeters) &&
        finite(q.state.velocityMapMetersPerSecond) &&
        finite(q.state.forwardMap) &&
        finite(q.state.rightMap) &&
        finite(q.state.upMap) &&
        finite(q.state.angularVelocityMapRadPerSecond) &&
        finite(q.geometricTargetPositionMapMeters) &&
        finite(q.desiredVelocityMapMetersPerSecond) &&
        finite(q.velocityResponsePerSecond) &&
        q.velocityResponsePerSecond >= 0.0 &&
        finite(q.linearFeedbackReserveMps2) &&
        q.linearFeedbackReserveMps2 >= 0.0 &&
        finite(q.angularFeedbackReserveRadPerSec2) &&
        q.angularFeedbackReserveRadPerSec2 >= 0.0 &&
        finite(q.controlResponseReserveSeconds) &&
        q.controlResponseReserveSeconds >= 0.0 &&
        finite(q.maximumProgramSeconds) &&
        q.maximumProgramSeconds >= kMinimumPrimitiveSeconds &&
        finite(c.maxForwardAccelerationMps2) &&
        c.maxForwardAccelerationMps2 >= 0.0 &&
        finite(c.maxReverseAccelerationMps2) &&
        c.maxReverseAccelerationMps2 >= 0.0 &&
        finite(c.maxLateralAccelerationMps2) &&
        c.maxLateralAccelerationMps2 >= 0.0 &&
        finite(c.maxVerticalAccelerationMps2) &&
        c.maxVerticalAccelerationMps2 >= 0.0 &&
        finite(c.maxAngularAccelerationRadPerSec2) &&
        c.maxAngularAccelerationRadPerSec2 >= 0.0 &&
        finite(c.maxAngularSpeedRadPerSec) &&
        c.maxAngularSpeedRadPerSec >= 0.0;
}

double usable(
    double authority,
    double reserve
) noexcept
{
    return std::max(0.0, authority - reserve);
}

bool bodyAxisFeasible(
    const glm::dvec3& acceleration,
    const glm::dvec3& forward,
    const glm::dvec3& right,
    const glm::dvec3& up,
    const Compiler::Capability& capability,
    double reserve,
    Candidate::RequiredAuthority& required
) noexcept
{
    const double forwardComponent =
        glm::dot(acceleration, forward);
    const double rightComponent =
        glm::dot(acceleration, right);
    const double upComponent =
        glm::dot(acceleration, up);

    required.peakForwardAccelerationMps2 =
        std::max(0.0, forwardComponent);
    required.peakReverseAccelerationMps2 =
        std::max(0.0, -forwardComponent);
    required.peakLateralAccelerationMps2 =
        std::abs(rightComponent);
    required.peakVerticalAccelerationMps2 =
        std::abs(upComponent);

    const double forwardAvailable =
        usable(capability.maxForwardAccelerationMps2, reserve);
    const double reverseAvailable =
        usable(capability.maxReverseAccelerationMps2, reserve);
    const double lateralAvailable =
        usable(capability.maxLateralAccelerationMps2, reserve);
    const double verticalAvailable =
        usable(capability.maxVerticalAccelerationMps2, reserve);

    return
        required.peakForwardAccelerationMps2 <=
            forwardAvailable + kEpsilon &&
        required.peakReverseAccelerationMps2 <=
            reverseAvailable + kEpsilon &&
        required.peakLateralAccelerationMps2 <=
            lateralAvailable + kEpsilon &&
        required.peakVerticalAccelerationMps2 <=
            verticalAvailable + kEpsilon;
}

void initializeCandidate(
    Candidate& candidate,
    Candidate::Family family,
    LocalFlightControlLaw law,
    const Compiler::Query& q
) noexcept
{
    candidate = {};
    candidate.valid = true;
    candidate.family = family;
    candidate.controlLaw = law;
    candidate.requestedTargetPositionMapMeters =
        q.geometricTargetPositionMapMeters;
    candidate.requestedTargetVelocityMapMetersPerSecond =
        q.desiredVelocityMapMetersPerSecond;
    candidate.requiresContinuousProof = true;
}

void emitConstantAcceleration(
    Candidate& candidate,
    const Compiler::Query& q,
    const glm::dvec3& forward,
    const glm::dvec3& right,
    const glm::dvec3& up,
    const glm::dvec3& acceleration,
    double duration
) noexcept
{
    candidate.sampleCount =
        static_cast<std::uint8_t>(Candidate::kMaxSamples);

    const double dt =
        duration /
        static_cast<double>(Candidate::kMaxSamples - 1);

    for (std::size_t i = 0; i < Candidate::kMaxSamples; ++i)
    {
        const double t = dt * static_cast<double>(i);
        auto& sample = candidate.samples[i];

        sample.timeOffsetSeconds = t;
        sample.positionMapMeters =
            q.state.positionMapMeters +
            q.state.velocityMapMetersPerSecond * t +
            0.5 * acceleration * t * t;
        sample.velocityMapMetersPerSecond =
            q.state.velocityMapMetersPerSecond +
            acceleration * t;
        sample.linearAccelerationFeedForwardMapMps2 =
            acceleration;

        sample.forwardMap = forward;
        sample.rightMap = right;
        sample.upMap = up;
        sample.angularVelocityMapRadPerSecond =
            q.state.angularVelocityMapRadPerSecond;
        sample.angularAccelerationFeedForwardMapRadPerSec2 =
            glm::dvec3(0.0);
    }
}

bool compileDirect(
    const Compiler::Query& q,
    const glm::dvec3& forward,
    const glm::dvec3& right,
    const glm::dvec3& up,
    Candidate& out
) noexcept
{
    const glm::dvec3 velocityError =
        q.desiredVelocityMapMetersPerSecond -
        q.state.velocityMapMetersPerSecond;

    glm::dvec3 requestedAcceleration =
        velocityError * q.velocityResponsePerSecond;

    const double duration =
        std::min(1.0, q.maximumProgramSeconds);

    if (duration <= kEpsilon)
        return false;

    // Never overshoot the requested delta-v inside this short primitive.
    const double errorMagnitude = glm::length(velocityError);
    const double accelMagnitude = glm::length(requestedAcceleration);
    if (errorMagnitude > kEpsilon &&
        accelMagnitude * duration > errorMagnitude)
    {
        requestedAcceleration =
            velocityError / duration;
    }

    Candidate::RequiredAuthority required;
    if (!bodyAxisFeasible(
            requestedAcceleration,
            forward,
            right,
            up,
            q.capability,
            q.linearFeedbackReserveMps2,
            required))
    {
        return false;
    }

    initializeCandidate(
        out,
        accelMagnitude <= kEpsilon
            ? Candidate::Family::Coast
            : Candidate::Family::Trim,
        q.controlLaw,
        q
    );
    out.required = required;

    emitConstantAcceleration(
        out,
        q,
        forward,
        right,
        up,
        requestedAcceleration,
        duration
    );
    return true;
}

bool compileLeadRotateMainBurn(
    const Compiler::Query& q,
    const glm::dvec3& forward,
    const glm::dvec3& right,
    const glm::dvec3& up,
    Candidate& out
) noexcept
{
    const glm::dvec3 deltaVelocity =
        q.desiredVelocityMapMetersPerSecond -
        q.state.velocityMapMetersPerSecond;
    const double deltaSpeed = glm::length(deltaVelocity);
    if (!finite(deltaSpeed) || deltaSpeed <= 1.0e-6)
        return false;

    const double forwardAvailable = usable(
        q.capability.maxForwardAccelerationMps2,
        q.linearFeedbackReserveMps2
    );
    const double angularAccelerationAvailable = usable(
        q.capability.maxAngularAccelerationRadPerSec2,
        q.angularFeedbackReserveRadPerSec2
    );
    const double angularSpeedAvailable =
        std::max(0.0, q.capability.maxAngularSpeedRadPerSec);

    if (forwardAvailable <= kEpsilon ||
        angularAccelerationAvailable <= kEpsilon ||
        angularSpeedAvailable <= kEpsilon)
    {
        return false;
    }

    const glm::dvec3 thrustDirection =
        deltaVelocity / deltaSpeed;

    const double cosAngle = std::clamp(
        glm::dot(forward, thrustDirection),
        -1.0,
        1.0
    );
    const double angle = std::acos(cosAngle);

    glm::dvec3 axis =
        glm::cross(forward, thrustDirection);
    const double axisLength = glm::length(axis);
    if (axisLength > kEpsilon)
    {
        axis /= axisLength;
    }
    else if (cosAngle < 0.0)
    {
        axis = up;
    }
    else
    {
        axis = up;
    }

    // Quintic rotation profile:
    // theta = angle * (10s^3 - 15s^4 + 6s^5)
    // conservative extrema:
    // max |omega| <= 1.875 * angle / T
    // max |alpha| < 6 * angle / T^2
    double rotateSeconds = 0.0;
    if (angle > kAngleEpsilon)
    {
        rotateSeconds = std::max(
            1.875 * angle / angularSpeedAvailable,
            std::sqrt(
                6.0 * angle /
                angularAccelerationAvailable
            )
        );
        rotateSeconds += q.controlResponseReserveSeconds;
    }

    const double rawBurnSeconds =
        deltaSpeed / forwardAvailable;
    const double burnRampSeconds =
        std::min(0.20, std::max(0.02, rawBurnSeconds * 0.25));
    double burnSeconds =
        rawBurnSeconds + 0.5 * burnRampSeconds;

    const double availableBurnWindow =
        q.maximumProgramSeconds - rotateSeconds;
    if (availableBurnWindow <= kMinimumPrimitiveSeconds)
        return false;

    burnSeconds = std::min(burnSeconds, availableBurnWindow);
    const double totalSeconds = rotateSeconds + burnSeconds;

    if (!finite(totalSeconds) ||
        totalSeconds < kMinimumPrimitiveSeconds)
    {
        return false;
    }

    initializeCandidate(
        out,
        Candidate::Family::LeadRotateMainBurn,
        q.controlLaw,
        q
    );

    out.sampleCount =
        static_cast<std::uint8_t>(Candidate::kMaxSamples);

    const double dt =
        totalSeconds /
        static_cast<double>(Candidate::kMaxSamples - 1);

    glm::dvec3 position = q.state.positionMapMeters;
    glm::dvec3 velocity = q.state.velocityMapMetersPerSecond;
    glm::dvec3 previousAcceleration(0.0);

    for (std::size_t i = 0; i < Candidate::kMaxSamples; ++i)
    {
        const double t = dt * static_cast<double>(i);

        double rotationS = 1.0;
        double rotationAngle = angle;
        double omegaMagnitude = 0.0;
        double alphaMagnitude = 0.0;

        if (rotateSeconds > kEpsilon && t < rotateSeconds)
        {
            rotationS = std::clamp(t / rotateSeconds, 0.0, 1.0);
            rotationAngle =
                angle * smoothQuintic(rotationS);
            omegaMagnitude =
                angle * smoothQuinticD1(rotationS) /
                rotateSeconds;
            alphaMagnitude =
                angle * smoothQuinticD2(rotationS) /
                (rotateSeconds * rotateSeconds);
        }
        else if (rotateSeconds <= kEpsilon)
        {
            rotationAngle = angle;
        }

        glm::dvec3 acceleration(0.0);
        if (t >= rotateSeconds)
        {
            const double burnT = t - rotateSeconds;
            const double ramp01 =
                burnRampSeconds > kEpsilon
                    ? std::clamp(
                          burnT / burnRampSeconds,
                          0.0,
                          1.0
                      )
                    : 1.0;
            const double ramp =
                ramp01 * ramp01 * (3.0 - 2.0 * ramp01);
            acceleration =
                thrustDirection *
                (forwardAvailable * ramp);
        }

        if (i > 0)
        {
            const glm::dvec3 averageAcceleration =
                0.5 * (previousAcceleration + acceleration);
            const glm::dvec3 previousVelocity = velocity;
            velocity += averageAcceleration * dt;
            position +=
                0.5 * (previousVelocity + velocity) * dt;
        }

        auto& sample = out.samples[i];
        sample.timeOffsetSeconds = t;
        sample.positionMapMeters = position;
        sample.velocityMapMetersPerSecond = velocity;
        sample.linearAccelerationFeedForwardMapMps2 =
            acceleration;

        sample.forwardMap =
            normalizedOr(
                rotateAroundAxis(
                    forward,
                    axis,
                    rotationAngle
                ),
                thrustDirection
            );
        sample.rightMap =
            normalizedOr(
                rotateAroundAxis(
                    right,
                    axis,
                    rotationAngle
                ),
                right
            );
        sample.upMap =
            normalizedOr(
                rotateAroundAxis(
                    up,
                    axis,
                    rotationAngle
                ),
                up
            );

        sample.angularVelocityMapRadPerSecond =
            axis * omegaMagnitude;
        sample.angularAccelerationFeedForwardMapRadPerSec2 =
            axis * alphaMagnitude;

        previousAcceleration = acceleration;

        out.required.peakForwardAccelerationMps2 =
            std::max(
                out.required.peakForwardAccelerationMps2,
                glm::dot(acceleration, sample.forwardMap)
            );
        out.required.peakAngularAccelerationRadPerSec2 =
            std::max(
                out.required.peakAngularAccelerationRadPerSec2,
                std::abs(alphaMagnitude)
            );
        out.required.peakAngularSpeedRadPerSec =
            std::max(
                out.required.peakAngularSpeedRadPerSec,
                std::abs(omegaMagnitude)
            );
    }

    // Numerical integration/sample spacing may leave the final velocity a bit
    // short of the requested delta-v. That is legal: B5 emits a bounded
    // receding-horizon maneuver, not a claim that the whole route is complete.
    return
        out.required.peakForwardAccelerationMps2 <=
            forwardAvailable + 1.0e-6 &&
        out.required.peakAngularAccelerationRadPerSec2 <=
            angularAccelerationAvailable + 1.0e-6 &&
        out.required.peakAngularSpeedRadPerSec <=
            angularSpeedAvailable + 1.0e-6;
}

} // namespace

OrdinaryPhysicalManeuverCompiler::Result
OrdinaryPhysicalManeuverCompiler::compile(
    const Query& query
) noexcept
{
    Result result;

    if (!validQuery(query))
        return result;

    if (query.controlLaw != LocalFlightControlLaw::Newtonian)
    {
        result.status = Status::UnsupportedControlLaw;
        return result;
    }

    glm::dvec3 forward;
    glm::dvec3 right;
    glm::dvec3 up;
    if (!orthonormalBasis(
            query.state.forwardMap,
            query.state.rightMap,
            query.state.upMap,
            forward,
            right,
            up))
    {
        return result;
    }

    Candidate direct;
    if (compileDirect(
            query,
            forward,
            right,
            up,
            direct))
    {
        result.candidates[result.candidateCount++] = direct;
        result.directBodyAxisFeasible = true;
    }

    const glm::dvec3 desiredAcceleration =
        (
            query.desiredVelocityMapMetersPerSecond -
            query.state.velocityMapMetersPerSecond
        ) * query.velocityResponsePerSecond;

    Candidate::RequiredAuthority rawRequired;
    const bool rawDirectFeasible =
        bodyAxisFeasible(
            desiredAcceleration,
            forward,
            right,
            up,
            query.capability,
            query.linearFeedbackReserveMps2,
            rawRequired
        );

    if (!rawDirectFeasible)
    {
        Candidate rotateBurn;
        if (compileLeadRotateMainBurn(
                query,
                forward,
                right,
                up,
                rotateBurn))
        {
            result.candidates[result.candidateCount++] =
                rotateBurn;
            result.leadRotateRequired = true;
        }
    }

    result.status =
        result.candidateCount > 0
            ? Status::Compiled
            : Status::NoPhysicalCandidate;
    return result;
}

} // namespace game::navigation
