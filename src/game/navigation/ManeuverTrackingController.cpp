#include "ManeuverTrackingController.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>

namespace game::navigation
{
namespace
{

constexpr double kEpsilon = 1.0e-12;

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite(const glm::dvec3& value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

bool nonNegativeFinite(double value) noexcept
{
    return finite(value) && value >= 0.0;
}

glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double lengthSquared = glm::dot(value, value);
    if (!finite(lengthSquared) || lengthSquared <= kEpsilon)
        return fallback;
    return value / std::sqrt(lengthSquared);
}

glm::dvec3 clampMagnitude(
    const glm::dvec3& value,
    double maximumMagnitude
) noexcept
{
    if (!(maximumMagnitude > 0.0))
        return glm::dvec3(0.0);

    const double magnitudeSquared = glm::dot(value, value);
    if (!finite(magnitudeSquared) || magnitudeSquared <= kEpsilon)
        return glm::dvec3(0.0);

    const double magnitude = std::sqrt(magnitudeSquared);
    if (magnitude <= maximumMagnitude)
        return value;

    return value * (maximumMagnitude / magnitude);
}

glm::dvec3 angularVelocityMap(
    const ManeuverTrackingController::AgentState& agent
) noexcept
{
    const glm::dvec3 forward = normalizedOr(
        agent.forwardMap,
        glm::dvec3(0.0, 0.0, -1.0)
    );
    const glm::dvec3 right = normalizedOr(
        agent.rightMap,
        glm::dvec3(1.0, 0.0, 0.0)
    );
    const glm::dvec3 up = normalizedOr(
        agent.upMap,
        glm::dvec3(0.0, 1.0, 0.0)
    );

    return
        right * agent.pitchRateRadPerSec +
        up * agent.yawRateRadPerSec +
        forward * agent.rollRateRadPerSec;
}

double angleBetween(
    const glm::dvec3& a,
    const glm::dvec3& b
) noexcept
{
    const glm::dvec3 na = normalizedOr(a, glm::dvec3(0.0, 0.0, -1.0));
    const glm::dvec3 nb = normalizedOr(b, na);
    return std::acos(std::clamp(glm::dot(na, nb), -1.0, 1.0));
}

// Exact shortest-arc SO(3) error using the complete body basis. The previous
// cross-product small-angle approximation becomes exactly zero at 180 degrees,
// so an antiparallel hull could never acquire an accepted attitude. Quaternion
// log space retains roll and remains well-defined for every non-degenerate
// body basis.
glm::dvec3 attitudeErrorVector(
    const AcceptedManeuverProgram::ReferenceSample& reference,
    const ManeuverTrackingController::AgentState& agent
) noexcept
{
    const glm::dvec3 currentForward = normalizedOr(
        agent.forwardMap,
        glm::dvec3(0.0, 0.0, -1.0)
    );
    const glm::dvec3 currentRight = normalizedOr(
        agent.rightMap,
        glm::dvec3(1.0, 0.0, 0.0)
    );
    const glm::dvec3 currentUp = normalizedOr(
        agent.upMap,
        glm::dvec3(0.0, 1.0, 0.0)
    );

    const glm::dvec3 targetForward =
        normalizedOr(reference.forwardMap, currentForward);
    const glm::dvec3 targetRight =
        normalizedOr(reference.rightMap, currentRight);
    const glm::dvec3 targetUp =
        normalizedOr(reference.upMap, currentUp);

    const glm::dquat current = glm::normalize(
        glm::quat_cast(
            glm::dmat3(
                currentRight,
                currentUp,
                -currentForward
            )
        )
    );
    const glm::dquat target = glm::normalize(
        glm::quat_cast(
            glm::dmat3(
                targetRight,
                targetUp,
                -targetForward
            )
        )
    );

    glm::dquat delta =
        glm::normalize(target * glm::conjugate(current));
    if (delta.w < 0.0)
        delta = -delta;

    const glm::dvec3 vectorPart(
        delta.x,
        delta.y,
        delta.z
    );
    const double vectorLength = glm::length(vectorPart);
    if (!finite(vectorLength) || vectorLength <= kEpsilon)
        return glm::dvec3(0.0);

    const double angle =
        2.0 * std::atan2(
            vectorLength,
            std::clamp(delta.w, 0.0, 1.0)
        );
    if (!finite(angle))
        return glm::dvec3(0.0);

    return vectorPart * (angle / vectorLength);
}

bool validPolicy(const ManeuverTrackingController::Policy& policy) noexcept
{
    return
        nonNegativeFinite(policy.positionGainPerSecond2) &&
        nonNegativeFinite(policy.velocityGainPerSecond) &&
        nonNegativeFinite(policy.attitudeGainPerSecond2) &&
        nonNegativeFinite(policy.angularVelocityGainPerSecond);
}

bool validInput(
    const AcceptedManeuverProgram& program,
    const AcceptedManeuverProgram::ReferenceSample& reference,
    const ManeuverTrackingController::AgentState& agent,
    const ManeuverTrackingController::Policy& policy
) noexcept
{
    return
        program.valid &&
        program.revision != 0 &&
        finite(reference.positionMapMeters) &&
        finite(reference.velocityMapMetersPerSecond) &&
        finite(reference.linearAccelerationFeedForwardMapMps2) &&
        finite(reference.forwardMap) &&
        finite(reference.rightMap) &&
        finite(reference.upMap) &&
        finite(reference.angularVelocityMapRadPerSecond) &&
        finite(reference.angularAccelerationFeedForwardMapRadPerSec2) &&
        finite(agent.positionMapMeters) &&
        finite(agent.velocityMapMetersPerSecond) &&
        finite(agent.forwardMap) &&
        finite(agent.rightMap) &&
        finite(agent.upMap) &&
        finite(agent.pitchRateRadPerSec) &&
        finite(agent.yawRateRadPerSec) &&
        finite(agent.rollRateRadPerSec) &&
        nonNegativeFinite(program.tracking.positionErrorMeters) &&
        nonNegativeFinite(program.tracking.linearVelocityErrorMps) &&
        nonNegativeFinite(program.tracking.forwardAngleErrorRad) &&
        nonNegativeFinite(program.tracking.angularVelocityErrorRadPerSec) &&
        nonNegativeFinite(program.tracking.alongTrackPositionDeadbandMeters) &&
        nonNegativeFinite(program.tracking.alongTrackSpeedDeadbandMps) &&
        nonNegativeFinite(program.tracking.spatialSlowdownStartFraction) &&
        program.tracking.spatialSlowdownStartFraction < 1.0 &&
        nonNegativeFinite(program.tracking.linearFeedbackReserveMps2) &&
        nonNegativeFinite(program.tracking.angularFeedbackReserveRadPerSec2) &&
        validPolicy(policy);
}

bool exceeded(double value, double maximum) noexcept
{
    return maximum > 0.0 && value > maximum;
}

double signedDeadbandExcess(
    double value,
    double halfWidth
) noexcept
{
    if (!(halfWidth > 0.0))
        return value;

    const double magnitude = std::abs(value);
    if (magnitude <= halfWidth)
        return 0.0;

    return std::copysign(
        magnitude - halfWidth,
        value
    );
}


} // namespace

ManeuverTrackingController::Result ManeuverTrackingController::track(
    const AcceptedManeuverProgram& program,
    const AcceptedManeuverProgram::ReferenceSample& reference,
    const AgentState& agent,
    const Policy& policy
) noexcept
{
    Result result;
    if (!validInput(program, reference, agent, policy))
        return result;

    const glm::dvec3 positionError =
        reference.positionMapMeters - agent.positionMapMeters;
    const glm::dvec3 velocityError =
        reference.velocityMapMetersPerSecond -
        agent.velocityMapMetersPerSecond;

    const glm::dvec3 actualAngularVelocity =
        angularVelocityMap(agent);
    const glm::dvec3 angularVelocityError =
        reference.angularVelocityMapRadPerSecond -
        actualAngularVelocity;

    result.positionErrorMeters = glm::length(positionError);
    result.linearVelocityErrorMps = glm::length(velocityError);
    result.forwardAngleErrorRad =
        angleBetween(agent.forwardMap, reference.forwardMap);
    double envelopeForwardAngleErrorRad =
        result.forwardAngleErrorRad;
    result.angularVelocityErrorRadPerSec =
        glm::length(angularVelocityError);

    if (!finite(result.positionErrorMeters) ||
        !finite(result.linearVelocityErrorMps) ||
        !finite(result.forwardAngleErrorRad) ||
        !finite(result.angularVelocityErrorRadPerSec))
    {
        return Result {};
    }

    glm::dvec3 effectivePositionError = positionError;
    glm::dvec3 effectiveVelocityError = velocityError;
    glm::dvec3 envelopePositionError = positionError;
    glm::dvec3 envelopeVelocityError = velocityError;

    const double referenceSpeedSquared =
        glm::dot(
            reference.velocityMapMetersPerSecond,
            reference.velocityMapMetersPerSecond
        );
    const bool spatialCorridor =
        program.referenceMode ==
            AcceptedManeuverProgram::ReferenceMode::SpatialCorridor;

    if (spatialCorridor)
    {
        glm::dvec3 tangent = reference.forwardMap;
        if (referenceSpeedSquared > kEpsilon)
        {
            tangent =
                reference.velocityMapMetersPerSecond /
                std::sqrt(referenceSpeedSquared);
        }
        else
        {
            const double forwardLength2 =
                glm::dot(tangent, tangent);
            if (!(forwardLength2 > kEpsilon) ||
                !finite(forwardLength2))
            {
                return Result {};
            }
            tangent /= std::sqrt(forwardLength2);
        }

        // Steering may deliberately point inward toward a look-ahead
        // point. Route-loss course is still measured against the authored
        // corridor tangent, otherwise the correction itself can invalidate
        // the route it is trying to recover.
        envelopeForwardAngleErrorRad =
            angleBetween(agent.forwardMap, tangent);

        const double alongPosition =
            glm::dot(positionError, tangent);
        const double alongVelocity =
            glm::dot(velocityError, tangent);

        const glm::dvec3 crossPosition =
            positionError - tangent * alongPosition;
        const glm::dvec3 crossVelocity =
            velocityError - tangent * alongVelocity;

        // Spatial progress already owns longitudinal position. Follower may
        // slow or accelerate along the corridor without declaring the path
        // lost. Longitudinal speed still receives bounded feedback, while the
        // execution envelope itself measures only escape from the tunnel.
        effectivePositionError = crossPosition;
        effectiveVelocityError =
            crossVelocity +
            tangent * signedDeadbandExcess(
                alongVelocity,
                program.tracking.alongTrackSpeedDeadbandMps
            );

        envelopePositionError = crossPosition;
        envelopeVelocityError = crossVelocity;
    }
    else if (program.family ==
                 AcceptedManeuverProgram::ManeuverFamily::FreeTransit &&
             referenceSpeedSquared > kEpsilon)
    {
        const glm::dvec3 tangent =
            reference.velocityMapMetersPerSecond /
            std::sqrt(referenceSpeedSquared);

        const double alongPosition =
            glm::dot(positionError, tangent);
        const double alongVelocity =
            glm::dot(velocityError, tangent);

        effectivePositionError =
            positionError -
            tangent * alongPosition +
            tangent * signedDeadbandExcess(
                alongPosition,
                program.tracking.
                    alongTrackPositionDeadbandMeters
            );

        effectiveVelocityError =
            velocityError -
            tangent * alongVelocity +
            tangent * signedDeadbandExcess(
                alongVelocity,
                program.tracking.
                    alongTrackSpeedDeadbandMps
            );

        envelopePositionError = effectivePositionError;
        envelopeVelocityError = effectiveVelocityError;
    }

    // TimeScheduled FreeTransit applies longitudinal deadbands to both control
    // and envelope. SpatialCorridor is different: longitudinal schedule drift
    // is a speed-control problem, while only cross-track P/V can mean the craft
    // has escaped the accepted tunnel.
    const double envelopePositionErrorMeters =
        glm::length(envelopePositionError);
    const double envelopeVelocityErrorMps =
        glm::length(envelopeVelocityError);
    result.envelopePositionErrorMeters = envelopePositionErrorMeters;
    result.envelopeVelocityErrorMps = envelopeVelocityErrorMps;
    result.envelopeForwardAngleErrorRad =
        envelopeForwardAngleErrorRad;

    const bool positionEnvelopeExceeded =
        exceeded(
            envelopePositionErrorMeters,
            program.tracking.positionErrorMeters
        );
    const bool velocityEnvelopeExceeded =
        exceeded(
            envelopeVelocityErrorMps,
            program.tracking.linearVelocityErrorMps
        );
    const bool courseEnvelopeExceeded =
        exceeded(
            envelopeForwardAngleErrorRad,
            program.tracking.forwardAngleErrorRad
        );
    const bool angularRateEnvelopeExceeded =
        exceeded(
            result.angularVelocityErrorRadPerSec,
            program.tracking.angularVelocityErrorRadPerSec
        );

    // In a spatial tunnel/canyon, route loss is geometric: cross-track
    // position/velocity and hull course must stay inside their envelopes.
    // Angular-rate mismatch is not a second clock.  While the nose is still
    // inside the allowed course cone, bounded angular feedback must remove the
    // spin without stopping translation or throwing the accepted route away.
    // TimeScheduled maneuvers retain the stricter dynamic-state envelope.
    const bool outsideEnvelope =
        positionEnvelopeExceeded ||
        velocityEnvelopeExceeded ||
        courseEnvelopeExceeded ||
        (!spatialCorridor && angularRateEnvelopeExceeded);

    result.angularCorrectionOnly =
        !positionEnvelopeExceeded &&
        !velocityEnvelopeExceeded &&
        !courseEnvelopeExceeded &&
        angularRateEnvelopeExceeded;

    const glm::dvec3 requestedLinearFeedback =
        effectivePositionError * policy.positionGainPerSecond2 +
        effectiveVelocityError * policy.velocityGainPerSecond;

    // Outside the proved tracking envelope the accepted feed-forward is no
    // longer authoritative. B10 falls back to bounded error reduction only;
    // orchestration may invalidate the program if that condition persists.
    const glm::dvec3 controlAngularVelocityError =
        outsideEnvelope && !result.angularCorrectionOnly
            ? -actualAngularVelocity
            : angularVelocityError;

    const glm::dvec3 requestedAngularFeedback =
        attitudeErrorVector(reference, agent) *
            policy.attitudeGainPerSecond2 +
        controlAngularVelocityError *
            policy.angularVelocityGainPerSecond;

    result.linearFeedbackMapMps2 = clampMagnitude(
        requestedLinearFeedback,
        program.tracking.linearFeedbackReserveMps2
    );
    result.angularFeedbackMapRadPerSec2 = clampMagnitude(
        requestedAngularFeedback,
        program.tracking.angularFeedbackReserveRadPerSec2
    );

    result.intent.revision = program.objectiveRevision;
    result.intent.targetRevision = program.revision;
    result.intent.emergency = program.emergency;
    result.intent.hazardUrgency01 =
        std::clamp(program.hazardUrgency01, 0.0, 1.0);

    const glm::dvec3 linearFeedForward =
        outsideEnvelope && !result.angularCorrectionOnly
            ? glm::dvec3(0.0)
            : reference.linearAccelerationFeedForwardMapMps2;
    const glm::dvec3 angularFeedForward =
        outsideEnvelope && !result.angularCorrectionOnly
            ? glm::dvec3(0.0)
            : reference.angularAccelerationFeedForwardMapRadPerSec2;

    result.intent.idealLinearAccelerationLocalMps2 =
        linearFeedForward +
        result.linearFeedbackMapMps2;
    result.intent.idealAngularAccelerationLocalRadPerSec2 =
        angularFeedForward +
        result.angularFeedbackMapRadPerSec2;

    if (program.capability.maxAngularAccelerationRadPerSec2 > 0.0)
    {
        result.intent.idealAngularAccelerationLocalRadPerSec2 =
            clampMagnitude(
                result.intent.idealAngularAccelerationLocalRadPerSec2,
                program.capability.maxAngularAccelerationRadPerSec2
            );
    }

    if (!finite(result.intent.idealLinearAccelerationLocalMps2) ||
        !finite(result.intent.idealAngularAccelerationLocalRadPerSec2))
    {
        return Result {};
    }

    result.status =
        outsideEnvelope
            ? Status::EnvelopeExceeded
            : Status::Tracking;
    return result;
}

} // namespace game::navigation
