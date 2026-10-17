#include "src/game/ship/plant/ShipPhysicalActuatorPlant.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace game::ship::plant
{
namespace
{

constexpr double Epsilon = 1.0e-12;
constexpr std::size_t MaxPhysicalSlots = 16;

EngineSlotRole activeLongitudinalBank(
    const glm::dvec3& accelerationBody
) noexcept
{
    // Canonical body basis: -Z forward, +Z aft.
    return accelerationBody.z <= 0.0
        ? EngineSlotRole::AftMain
        : EngineSlotRole::ForeMain;
}

double positiveFiniteOr(double value, double fallback) noexcept
{
    return std::isfinite(value) && value > Epsilon
        ? value
        : fallback;
}

std::uint64_t mix64(std::uint64_t value) noexcept
{
    value ^= value >> 30u;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27u;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31u;
    return value;
}

double deterministicUnitSample(
    std::size_t slotIndex,
    std::uint64_t controlTick
) noexcept
{
    // Hold one sample for four fixed-control ticks so the symptom is a
    // perceptible thrust hiccup rather than high-frequency numerical noise.
    const std::uint64_t bucket = controlTick / 4u;
    const std::uint64_t seed =
        bucket ^
        (0x9e3779b97f4a7c15ULL *
            (static_cast<std::uint64_t>(slotIndex) + 1u));

    const std::uint64_t bits = mix64(seed);
    return static_cast<double>(bits >> 11u) *
        (1.0 / 9007199254740992.0);
}

struct AllocationEvaluation
{
    bool valid = false;
    glm::dvec3 accelerationBodyMps2 {0.0};
    MainPropulsionReaction reaction;
    double feedbackThrustScale01 = 1.0;
};

glm::dvec3 harmfulAngularAcceleration(
    const glm::dvec3& alpha,
    const glm::dvec3& attitudeIntent01
) noexcept
{
    glm::dvec3 out(0.0);
    for (int axis = 0; axis < 3; ++axis)
    {
        const double intent =
            std::clamp(attitudeIntent01[axis], -1.0, 1.0);

        // With no requested turn, every parasitic component is harmful.
        // During a requested turn, only reaction that opposes that turn is
        // trimmed. Helpful reaction is left for the ordinary attitude feedback
        // loop to account for on the next control sample.
        if (std::abs(intent) <= 1.0e-3 ||
            alpha[axis] * intent < 0.0)
        {
            out[axis] = alpha[axis];
        }
    }
    return out;
}

double rawHarmfulMetric(
    const MainPropulsionReaction& reaction,
    const glm::dvec3& attitudeIntent01
) noexcept
{
    const glm::dvec3 harmful =
        harmfulAngularAcceleration(
            reaction.angularAccelerationBodyRadPerSec2,
            attitudeIntent01
        );
    return glm::dot(harmful, harmful);
}

double normalizedHarmfulMetric(
    const AllocationEvaluation& allocation,
    const glm::dvec3& attitudeIntent01
) noexcept
{
    const double raw =
        rawHarmfulMetric(
            allocation.reaction,
            attitudeIntent01
        );
    const double acceleration =
        glm::length(allocation.accelerationBodyMps2);

    // Uniformly shutting down an already-unbalanced bank is not compensation:
    // it reduces useful force and parasitic torque in the same proportion.
    // Normalize by delivered main acceleration so throttle feedback is accepted
    // only when the maneuver becomes cleaner, not merely weaker.
    return raw /
        std::max(acceleration * acceleration, 1.0e-12);
}

glm::dvec3 applicationPointBodyMeters(
    const EngineSlotDefinition& slot,
    const ShipParams& physics
) noexcept
{
    const glm::dvec3 configuredHalfExtents =
        physics.physicalActuatorHalfExtentsMeters;
    const bool hasPhysicalGeometry =
        configuredHalfExtents.x > Epsilon &&
        configuredHalfExtents.y > Epsilon &&
        configuredHalfExtents.z > Epsilon;

    const double legacyLeverArmMeters =
        std::max(0.0, static_cast<double>(physics.turnRadius));
    const glm::dvec3 halfExtentsMeters =
        hasPhysicalGeometry
            ? configuredHalfExtents
            : glm::dvec3(legacyLeverArmMeters);

    return {
        slot.normalizedPositionBody.x * halfExtentsMeters.x,
        slot.normalizedPositionBody.y * halfExtentsMeters.y,
        slot.normalizedPositionBody.z * halfExtentsMeters.z
    };
}

AllocationEvaluation evaluateTrimmedAllocation(
    PhysicalControlModelId model,
    const std::vector<EngineSlotState>& state,
    const ShipParams& physics,
    const glm::dvec3& requestedAccelerationBodyMps2,
    const std::array<double, MaxPhysicalSlots>& throttleScale01,
    double yawTrimRad,
    double pitchTrimRad,
    std::uint64_t controlTick
) noexcept
{
    AllocationEvaluation out;
    const auto& slots = physicalControlSlots(model);
    if (state.size() != slots.size() ||
        slots.size() > MaxPhysicalSlots)
    {
        return out;
    }

    const double requestedMagnitude =
        glm::length(requestedAccelerationBodyMps2);
    out.valid = true;
    out.reaction.valid = true;
    if (requestedMagnitude <= Epsilon)
        return out;

    const EngineSlotRole activeRole =
        activeLongitudinalBank(requestedAccelerationBodyMps2);

    double steadyTotal = 0.0;
    double instantaneousTotal = 0.0;
    double commandedTotal = 0.0;
    for (std::size_t i = 0; i < slots.size(); ++i)
    {
        if (slots[i].role != activeRole)
            continue;

        steadyTotal += effectiveSlotThrust01(state[i]);

        const double instantaneous =
            instantaneousSlotThrust01(state[i], i, controlTick);
        instantaneousTotal += instantaneous;
        commandedTotal +=
            instantaneous *
            std::clamp(throttleScale01[i], 0.0, 1.0);
    }

    if (steadyTotal <= Epsilon || commandedTotal <= Epsilon)
    {
        out.feedbackThrustScale01 =
            instantaneousTotal > Epsilon ? 0.0 : 1.0;
        return out;
    }

    glm::dvec3 direction =
        requestedAccelerationBodyMps2 / requestedMagnitude;

    // Small common automatic TVC correction. The ordinary flight-law/pilot
    // direction remains the base; automatic trim only adds a bounded offset.
    direction += glm::dvec3(
        std::tan(yawTrimRad),
        std::tan(pitchTrimRad),
        0.0
    );
    const double directionLength = glm::length(direction);
    if (!std::isfinite(directionLength) ||
        directionLength <= Epsilon)
    {
        return {};
    }
    direction /= directionLength;

    const double accelerationScale =
        std::clamp(commandedTotal / steadyTotal, 0.0, 1.0);
    out.accelerationBodyMps2 =
        direction * (requestedMagnitude * accelerationScale);
    out.feedbackThrustScale01 =
        instantaneousTotal > Epsilon
            ? std::clamp(
                  commandedTotal / instantaneousTotal,
                  0.0,
                  1.0
              )
            : 1.0;

    const double massKg =
        positiveFiniteOr(physics.massKg, 1.0);
    const glm::dvec3 centerOfMass =
        physics.centerOfMassLocalMeters;

    glm::dvec3 torque(0.0);
    for (std::size_t i = 0; i < slots.size(); ++i)
    {
        if (slots[i].role != activeRole)
            continue;

        const double instantaneous =
            instantaneousSlotThrust01(state[i], i, controlTick);
        const double commandedWeight =
            instantaneous *
            std::clamp(throttleScale01[i], 0.0, 1.0);
        if (commandedWeight <= Epsilon)
            continue;

        const glm::dvec3 armBodyMeters =
            applicationPointBodyMeters(slots[i], physics) -
            centerOfMass;

        // requestedMagnitude/steadyTotal is the acceleration contribution of
        // one unit of the original static bank authority. Reducing one engine
        // therefore reduces the real net force instead of renormalizing the
        // remaining engines back up to the old total.
        const glm::dvec3 slotForceBodyNewtons =
            direction *
            (requestedMagnitude * massKg *
             (commandedWeight / steadyTotal));

        torque += glm::cross(
            armBodyMeters,
            slotForceBodyNewtons
        );
    }

    out.reaction.torqueBodyNm = torque;

    const double pitchInertia =
        positiveFiniteOr(physics.pitchInertiaKgM2, 1.0);
    const double yawInertia =
        positiveFiniteOr(physics.yawInertiaKgM2, 1.0);
    const double rollInertia =
        positiveFiniteOr(physics.rollInertiaKgM2, 1.0);

    out.reaction.angularAccelerationBodyRadPerSec2 = {
        torque.x / pitchInertia,
        torque.y / yawInertia,
        -torque.z / rollInertia
    };

    return out;
}

} // namespace

double instantaneousSlotThrust01(
    const EngineSlotState& state,
    std::size_t slotIndex,
    std::uint64_t controlTick
) noexcept
{
    const double base = effectiveSlotThrust01(state);
    if (base <= Epsilon)
        return 0.0;

    const double irregularity =
        std::clamp(state.thrustIrregularity01, 0.0, 1.0);
    if (irregularity <= Epsilon)
        return base;

    const double sample =
        deterministicUnitSample(slotIndex, controlTick);

    // Irregularity only removes instantaneous thrust. It never fabricates
    // authority above the damage-limited maximum. As damage approaches the
    // 95% failure threshold, dips can approach zero.
    const double multiplier =
        (1.0 - irregularity) + irregularity * sample;

    return base * std::clamp(multiplier, 0.0, 1.0);
}

double evaluateMainPropulsionInstantaneousScale(
    PhysicalControlModelId model,
    const std::vector<EngineSlotState>& state,
    const glm::dvec3& mainAccelerationBodyMps2,
    std::uint64_t controlTick
) noexcept
{
    const auto& slots = physicalControlSlots(model);
    if (state.size() != slots.size())
        return 1.0;

    if (glm::length(mainAccelerationBodyMps2) <= Epsilon)
        return 1.0;

    const EngineSlotRole activeRole =
        activeLongitudinalBank(mainAccelerationBodyMps2);

    double steadyTotal = 0.0;
    double instantaneousTotal = 0.0;

    for (std::size_t i = 0; i < slots.size(); ++i)
    {
        if (slots[i].role != activeRole)
            continue;

        steadyTotal += effectiveSlotThrust01(state[i]);
        instantaneousTotal +=
            instantaneousSlotThrust01(
                state[i],
                i,
                controlTick
            );
    }

    if (steadyTotal <= Epsilon)
        return 1.0;

    return std::clamp(
        instantaneousTotal / steadyTotal,
        0.0,
        1.0
    );
}

MainPropulsionTrimResult evaluateMainPropulsionFeedbackTrim(
    PhysicalControlModelId model,
    const std::vector<EngineSlotState>& state,
    const ShipParams& physics,
    const glm::dvec3& requestedAccelerationBodyMps2,
    const glm::dvec3& attitudeIntent01,
    double remainingYawTrimRad,
    double remainingPitchTrimRad,
    std::uint64_t controlTick
) noexcept
{
    MainPropulsionTrimResult out;

    const auto& slots = physicalControlSlots(model);
    if (state.size() != slots.size())
        return out;

    if (!std::isfinite(requestedAccelerationBodyMps2.x) ||
        !std::isfinite(requestedAccelerationBodyMps2.y) ||
        !std::isfinite(requestedAccelerationBodyMps2.z))
    {
        return out;
    }

    const double requestedMagnitude =
        glm::length(requestedAccelerationBodyMps2);

    out.valid = true;
    out.reaction.valid = true;
    if (requestedMagnitude <= Epsilon)
        return out;

    const EngineSlotRole activeRole =
        activeLongitudinalBank(requestedAccelerationBodyMps2);

    if (slots.size() > MaxPhysicalSlots)
        return {};

    std::array<double, MaxPhysicalSlots> throttleScale01 {};
    throttleScale01.fill(1.0);

    std::size_t activeMainCount = 0;
    double commonVectoring01 = 1.0;
    bool hasVectoringMain = false;

    for (std::size_t i = 0; i < slots.size(); ++i)
    {
        if (slots[i].role != activeRole)
            continue;

        const double instantaneous =
            instantaneousSlotThrust01(state[i], i, controlTick);
        if (instantaneous <= Epsilon)
            continue;

        ++activeMainCount;

        if (slots[i].freedom == EngineSlotFreedom::VectoringMain)
        {
            hasVectoringMain = true;
            const double thrust =
                std::max(effectiveSlotThrust01(state[i]), Epsilon);
            const double relativeVectoring =
                std::clamp(
                    effectiveSlotVectoring01(state[i]) / thrust,
                    0.0,
                    1.0
                );
            commonVectoring01 =
                std::min(commonVectoring01, relativeVectoring);
        }
        else
        {
            commonVectoring01 = 0.0;
        }
    }

    auto current =
        evaluateTrimmedAllocation(
            model,
            state,
            physics,
            requestedAccelerationBodyMps2,
            throttleScale01,
            0.0,
            0.0,
            controlTick
        );
    if (!current.valid)
        return {};

    constexpr double AngularMetricStop = 1.0e-10;

    // Cheap coordinate descent: at most eight 10% cuts, and only when more
    // than one main engine is available to redistribute. Healthy symmetric
    // banks exit immediately without entering this loop.
    if (activeMainCount > 1)
    {
        for (int iteration = 0; iteration < 8; ++iteration)
        {
            const double currentRawMetric =
                rawHarmfulMetric(
                    current.reaction,
                    attitudeIntent01
                );
            if (currentRawMetric <= AngularMetricStop)
                break;

            const double currentMetric =
                normalizedHarmfulMetric(
                    current,
                    attitudeIntent01
                );

            std::size_t bestSlot = slots.size();
            double bestQuality = 0.0;
            AllocationEvaluation bestCandidate;

            for (std::size_t i = 0; i < slots.size(); ++i)
            {
                if (slots[i].role != activeRole ||
                    throttleScale01[i] <= Epsilon)
                {
                    continue;
                }

                const double oldScale = throttleScale01[i];
                const double newScale =
                    std::max(0.0, oldScale - 0.10);
                if (newScale >= oldScale - Epsilon)
                    continue;

                throttleScale01[i] = newScale;
                const auto candidate =
                    evaluateTrimmedAllocation(
                        model,
                        state,
                        physics,
                        requestedAccelerationBodyMps2,
                        throttleScale01,
                        0.0,
                        0.0,
                        controlTick
                    );
                throttleScale01[i] = oldScale;

                if (!candidate.valid)
                    continue;

                const double candidateMetric =
                    normalizedHarmfulMetric(
                        candidate,
                        attitudeIntent01
                    );
                const double improvement =
                    currentMetric - candidateMetric;
                if (improvement <= 1.0e-14)
                    continue;

                const double lostThrust =
                    instantaneousSlotThrust01(
                        state,
                        i,
                        controlTick
                    ) * (oldScale - newScale);

                const double quality =
                    improvement /
                    std::max(lostThrust, 1.0e-9);

                if (quality > bestQuality)
                {
                    bestQuality = quality;
                    bestSlot = i;
                    bestCandidate = candidate;
                }
            }

            if (bestSlot == slots.size())
                break;

            throttleScale01[bestSlot] =
                std::max(
                    0.0,
                    throttleScale01[bestSlot] - 0.10
                );
            current = bestCandidate;
        }
    }

    // One tiny common-TVC feedback stage after throttle balancing. It is
    // deliberately conservative and bounded to two degrees. If any active
    // engine in the bank has lost TVC completely, common trim is disabled
    // rather than pretending all nozzles can follow the same command.
    constexpr double MaxAutomaticTvcTrimRad =
        0.03490658503988659154; // 2 deg

    const double yawLimit =
        hasVectoringMain
            ? std::min(
                  std::abs(remainingYawTrimRad) * commonVectoring01,
                  MaxAutomaticTvcTrimRad
              )
            : 0.0;
    const double pitchLimit =
        hasVectoringMain
            ? std::min(
                  std::abs(remainingPitchTrimRad) * commonVectoring01,
                  MaxAutomaticTvcTrimRad
              )
            : 0.0;

    double yawTrim = 0.0;
    double pitchTrim = 0.0;
    double yawStep = yawLimit * 0.5;
    double pitchStep = pitchLimit * 0.5;

    for (int iteration = 0; iteration < 2; ++iteration)
    {
        const double currentRawMetric =
            rawHarmfulMetric(
                current.reaction,
                attitudeIntent01
            );
        if (currentRawMetric <= AngularMetricStop)
            break;

        const double currentMetric =
            normalizedHarmfulMetric(
                current,
                attitudeIntent01
            );

        AllocationEvaluation bestCandidate = current;
        double bestMetric = currentMetric;
        double bestYaw = yawTrim;
        double bestPitch = pitchTrim;

        const auto tryCandidate =
            [&](double candidateYaw, double candidatePitch)
            {
                const auto candidate =
                    evaluateTrimmedAllocation(
                        model,
                        state,
                        physics,
                        requestedAccelerationBodyMps2,
                        throttleScale01,
                        candidateYaw,
                        candidatePitch,
                        controlTick
                    );
                if (!candidate.valid)
                    return;

                const double metric =
                    normalizedHarmfulMetric(
                        candidate,
                        attitudeIntent01
                    );

                // Do not bend the requested force direction for a negligible
                // improvement. Five percent is enough to keep this feedback
                // useful without turning it into an optimizer.
                if (metric < bestMetric * 0.95)
                {
                    bestMetric = metric;
                    bestCandidate = candidate;
                    bestYaw = candidateYaw;
                    bestPitch = candidatePitch;
                }
            };

        if (yawStep > Epsilon)
        {
            tryCandidate(
                std::clamp(yawTrim + yawStep, -yawLimit, yawLimit),
                pitchTrim
            );
            tryCandidate(
                std::clamp(yawTrim - yawStep, -yawLimit, yawLimit),
                pitchTrim
            );
        }

        if (pitchStep > Epsilon)
        {
            tryCandidate(
                yawTrim,
                std::clamp(
                    pitchTrim + pitchStep,
                    -pitchLimit,
                    pitchLimit
                )
            );
            tryCandidate(
                yawTrim,
                std::clamp(
                    pitchTrim - pitchStep,
                    -pitchLimit,
                    pitchLimit
                )
            );
        }

        if (bestMetric >= currentMetric * 0.95)
            break;

        current = bestCandidate;
        yawTrim = bestYaw;
        pitchTrim = bestPitch;
        yawStep *= 0.5;
        pitchStep *= 0.5;
    }

    out.accelerationBodyMps2 = current.accelerationBodyMps2;
    out.reaction = current.reaction;
    out.thrustScale01 = current.feedbackThrustScale01;
    out.automaticYawTrimRad = yawTrim;
    out.automaticPitchTrimRad = pitchTrim;

    const double yawUse =
        yawLimit > Epsilon
            ? std::abs(yawTrim) / yawLimit
            : 0.0;
    const double pitchUse =
        pitchLimit > Epsilon
            ? std::abs(pitchTrim) / pitchLimit
            : 0.0;

    out.trimLoad01 =
        std::clamp(
            std::max({
                1.0 - out.thrustScale01,
                yawUse,
                pitchUse
            }),
            0.0,
            1.0
        );

    return out;
}

MainPropulsionReaction evaluateMainPropulsionReaction(
    PhysicalControlModelId model,
    const std::vector<EngineSlotState>& state,
    const ShipParams& physics,
    const glm::dvec3& mainAccelerationBodyMps2,
    std::uint64_t controlTick
) noexcept
{
    MainPropulsionReaction out;

    const auto& slots = physicalControlSlots(model);
    if (state.size() != slots.size())
        return out;

    if (!std::isfinite(mainAccelerationBodyMps2.x) ||
        !std::isfinite(mainAccelerationBodyMps2.y) ||
        !std::isfinite(mainAccelerationBodyMps2.z))
    {
        return out;
    }

    out.valid = true;

    if (glm::length(mainAccelerationBodyMps2) <= Epsilon)
        return out;

    const EngineSlotRole activeRole =
        activeLongitudinalBank(mainAccelerationBodyMps2);

    double totalWeight = 0.0;
    for (std::size_t i = 0; i < slots.size(); ++i)
    {
        if (slots[i].role != activeRole)
            continue;
        totalWeight += instantaneousSlotThrust01(
            state[i],
            i,
            controlTick
        );
    }

    if (totalWeight <= Epsilon)
        return out;

    const double massKg =
        positiveFiniteOr(physics.massKg, 1.0);

    const glm::dvec3 configuredHalfExtents =
        physics.physicalActuatorHalfExtentsMeters;
    const bool hasPhysicalGeometry =
        configuredHalfExtents.x > Epsilon &&
        configuredHalfExtents.y > Epsilon &&
        configuredHalfExtents.z > Epsilon;

    // Legacy descriptors/tests that have not yet supplied logical dimensions
    // retain a conservative isotropic fallback. Production capability
    // projection supplies true X/Y/Z half-extents from the descriptor.
    const double legacyLeverArmMeters =
        std::max(0.0, static_cast<double>(physics.turnRadius));
    const glm::dvec3 halfExtentsMeters =
        hasPhysicalGeometry
            ? configuredHalfExtents
            : glm::dvec3(legacyLeverArmMeters);

    const glm::dvec3 centerOfMass =
        physics.centerOfMassLocalMeters;

    glm::dvec3 torque(0.0);

    for (std::size_t i = 0; i < slots.size(); ++i)
    {
        if (slots[i].role != activeRole)
            continue;

        const double weight = instantaneousSlotThrust01(
            state[i],
            i,
            controlTick
        );
        if (weight <= Epsilon)
            continue;

        const double fraction = weight / totalWeight;

        const glm::dvec3 applicationPointBodyMeters(
            slots[i].normalizedPositionBody.x * halfExtentsMeters.x,
            slots[i].normalizedPositionBody.y * halfExtentsMeters.y,
            slots[i].normalizedPositionBody.z * halfExtentsMeters.z
        );

        const glm::dvec3 armBodyMeters =
            applicationPointBodyMeters - centerOfMass;

        const glm::dvec3 slotForceBodyNewtons =
            mainAccelerationBodyMps2 * (massKg * fraction);

        torque += glm::cross(
            armBodyMeters,
            slotForceBodyNewtons
        );
    }

    out.torqueBodyNm = torque;

    const double pitchInertia =
        positiveFiniteOr(physics.pitchInertiaKgM2, 1.0);
    const double yawInertia =
        positiveFiniteOr(physics.yawInertiaKgM2, 1.0);
    const double rollInertia =
        positiveFiniteOr(physics.rollInertiaKgM2, 1.0);

    out.angularAccelerationBodyRadPerSec2 = {
        torque.x / pitchInertia,
        torque.y / yawInertia,

        // ShipTransform::rollRate is positive about ship forward, while the
        // canonical body Z axis points aft.
        -torque.z / rollInertia
    };

    return out;
}

} // namespace game::ship::plant
