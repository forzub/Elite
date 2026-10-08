#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "src/game/navigation/planner/RoutePlannerApi.h"

namespace game::navigation
{

// Rotation-minimizing (Bishop-style) frame field for route presentation.
//
// The field is built from the authoritative parametric routeCurves, not from
// HUD gate spacing.  Sparse/dense gate sets therefore sample one canonical
// orientation field instead of reconstructing "up" independently at every
// visible marker.
class RouteFrameField
{
public:
    struct Sample
    {
        double progressMeters = 0.0;
        glm::dvec3 positionMeters {0.0};
        glm::dvec3 forward {0.0, 0.0, -1.0};
        glm::dvec3 right {1.0, 0.0, 0.0};
        glm::dvec3 up {0.0, 1.0, 0.0};
        glm::dquat orientation {1.0, 0.0, 0.0, 0.0};
    };

    [[nodiscard]] static std::vector<Sample> build(
        const std::vector<planner::RouteCurveSegment>& curves,
        const glm::dvec3& initialUpReference,
        double canonicalStepMeters = 25.0
    )
    {
        std::vector<Sample> out;
        if (curves.empty())
            return out;

        const double step = std::max(1.0, canonicalStepMeters);

        auto appendGeometrySample =
            [&](double progressMeters,
                const planner::RouteCurveSegment& curve)
            {
                if (!out.empty() &&
                    std::abs(out.back().progressMeters - progressMeters) <=
                        1.0e-9)
                {
                    return;
                }

                Sample sample;
                sample.progressMeters = progressMeters;
                const double t = curve.parameterAtProgress(progressMeters);
                sample.positionMeters = curve.positionAtParameter(t);
                sample.forward = normalizedOr(
                    curve.tangentAtProgress(progressMeters),
                    out.empty()
                        ? curve.startForward
                        : out.back().forward
                );
                out.push_back(sample);
            };

        for (const auto& curve : curves)
        {
            const double begin = curve.startProgressMeters;
            const double end = curve.endProgressMeters;
            if (!(end >= begin))
                continue;

            appendGeometrySample(begin, curve);

            for (double s = begin + step; s < end - 1.0e-9; s += step)
                appendGeometrySample(s, curve);

            appendGeometrySample(end, curve);
        }

        if (out.empty())
            return out;

        glm::dvec3 firstUp = initialUpReference;
        if (glm::length(firstUp) <= 1.0e-9)
            firstUp = glm::dvec3(0.0, 1.0, 0.0);

        firstUp -= out.front().forward *
            glm::dot(firstUp, out.front().forward);
        if (glm::length(firstUp) <= 1.0e-9)
            firstUp = perpendicularSeed(out.front().forward);
        firstUp = glm::normalize(firstUp);

        setFrame(out.front(), firstUp);

        for (std::size_t i = 1; i < out.size(); ++i)
        {
            const glm::dvec3 transported =
                minimalRotate(
                    out[i - 1].forward,
                    out[i].forward,
                    out[i - 1].up
                );

            glm::dvec3 up =
                transported -
                out[i].forward *
                    glm::dot(transported, out[i].forward);
            if (glm::length(up) <= 1.0e-9)
                up = perpendicularSeed(out[i].forward);
            up = glm::normalize(up);

            // Quaternion/vector signs are arbitrary representations, but the
            // visible basis must not switch to its opposite branch.
            if (glm::dot(up, out[i - 1].up) < 0.0)
                up = -up;

            setFrame(out[i], up);
        }

        return out;
    }

    [[nodiscard]] static std::vector<glm::dvec3> sampleUpForGates(
        const std::vector<Sample>& field,
        const std::vector<planner::RouteGate>& gates
    )
    {
        std::vector<glm::dvec3> result;
        result.reserve(gates.size());
        if (field.empty())
            return result;

        std::size_t searchBegin = 0;
        for (const auto& gate : gates)
        {
            std::size_t best = searchBegin;
            double bestDistance2 =
                std::numeric_limits<double>::infinity();

            for (std::size_t i = searchBegin; i < field.size(); ++i)
            {
                const glm::dvec3 delta =
                    gate.positionMeters - field[i].positionMeters;
                const double distance2 = glm::dot(delta, delta);
                if (distance2 < bestDistance2)
                {
                    bestDistance2 = distance2;
                    best = i;
                }
            }

            searchBegin = best;

            double estimatedProgress = field[best].progressMeters;
            estimatedProgress += glm::dot(
                gate.positionMeters - field[best].positionMeters,
                field[best].forward
            );
            estimatedProgress = std::clamp(
                estimatedProgress,
                field.front().progressMeters,
                field.back().progressMeters
            );

            glm::dvec3 up = upAtProgress(field, estimatedProgress);
            const glm::dvec3 forward =
                normalizedOr(gate.forward, field[best].forward);
            up -= forward * glm::dot(up, forward);
            if (glm::length(up) <= 1.0e-9)
                up = perpendicularSeed(forward);
            result.push_back(glm::normalize(up));
        }

        return result;
    }

    [[nodiscard]] static glm::dvec3 upAtProgress(
        const std::vector<Sample>& field,
        double progressMeters
    )
    {
        if (field.empty())
            return glm::dvec3(0.0, 1.0, 0.0);
        if (field.size() == 1 ||
            progressMeters <= field.front().progressMeters)
        {
            return field.front().up;
        }
        if (progressMeters >= field.back().progressMeters)
            return field.back().up;

        const auto upper = std::upper_bound(
            field.begin(),
            field.end(),
            progressMeters,
            [](double value, const Sample& sample)
            {
                return value < sample.progressMeters;
            }
        );
        const std::size_t hi =
            static_cast<std::size_t>(upper - field.begin());
        const std::size_t lo = hi - 1;

        const double span =
            field[hi].progressMeters - field[lo].progressMeters;
        const double u =
            span > 1.0e-12
                ? std::clamp(
                    (progressMeters - field[lo].progressMeters) / span,
                    0.0,
                    1.0
                  )
                : 0.0;

        glm::dquat a = field[lo].orientation;
        glm::dquat b = field[hi].orientation;
        if (glm::dot(a, b) < 0.0)
            b = -b;

        const glm::dquat q =
            glm::normalize(glm::slerp(a, b, u));
        return q * glm::dvec3(0.0, 1.0, 0.0);
    }

private:
    [[nodiscard]] static glm::dvec3 normalizedOr(
        const glm::dvec3& value,
        const glm::dvec3& fallback
    )
    {
        const double length = glm::length(value);
        if (std::isfinite(length) && length > 1.0e-12)
            return value / length;

        const double fallbackLength = glm::length(fallback);
        if (std::isfinite(fallbackLength) && fallbackLength > 1.0e-12)
            return fallback / fallbackLength;

        return glm::dvec3(0.0, 0.0, -1.0);
    }

    [[nodiscard]] static glm::dvec3 perpendicularSeed(
        const glm::dvec3& forward
    )
    {
        const glm::dvec3 seed =
            std::abs(forward.y) < 0.90
                ? glm::dvec3(0.0, 1.0, 0.0)
                : glm::dvec3(1.0, 0.0, 0.0);
        return glm::normalize(
            seed - forward * glm::dot(seed, forward)
        );
    }

    [[nodiscard]] static glm::dvec3 minimalRotate(
        const glm::dvec3& fromForward,
        const glm::dvec3& toForward,
        const glm::dvec3& vector
    )
    {
        const glm::dvec3 from = normalizedOr(
            fromForward,
            glm::dvec3(0.0, 0.0, -1.0)
        );
        const glm::dvec3 to = normalizedOr(toForward, from);

        const double cosine =
            std::clamp(glm::dot(from, to), -1.0, 1.0);
        const glm::dvec3 crossAxis = glm::cross(from, to);
        const double sine = glm::length(crossAxis);

        if (sine <= 1.0e-12)
        {
            if (cosine >= 0.0)
                return vector;

            const glm::dvec3 axis = perpendicularSeed(from);
            return glm::angleAxis(glm::pi<double>(), axis) * vector;
        }

        const glm::dvec3 axis = crossAxis / sine;
        const double angle = std::atan2(sine, cosine);
        return glm::angleAxis(angle, axis) * vector;
    }

    static void setFrame(
        Sample& sample,
        const glm::dvec3& requestedUp
    )
    {
        glm::dvec3 up =
            requestedUp -
            sample.forward *
                glm::dot(requestedUp, sample.forward);
        if (glm::length(up) <= 1.0e-9)
            up = perpendicularSeed(sample.forward);
        up = glm::normalize(up);

        sample.right =
            glm::normalize(glm::cross(sample.forward, up));
        sample.up =
            glm::normalize(glm::cross(sample.right, sample.forward));
        sample.orientation =
            glm::normalize(
                glm::quat_cast(
                    glm::dmat3(
                        sample.right,
                        sample.up,
                        -sample.forward
                    )
                )
            );
    }
};

} // namespace game::navigation
