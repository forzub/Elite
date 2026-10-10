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

#include "src/game/navigation/TwoPointRollGeometry.h"
#include "src/game/navigation/planner/RoutePlannerApi.h"

namespace game::navigation
{

// Rotation-minimizing (Bishop-style) frame field for route presentation.
//
// The field is built from the authoritative parametric routeCurves, not from
// HUD gate spacing.  Sparse/dense gate sets therefore sample one canonical
// orientation field instead of reconstructing "up" independently at every
// visible marker. Absolute roll phase is anchored at the terminal docking
// frame and transported backwards through the route.
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

        // How much of the live terminal dock roll phase belongs to this
        // station. Traffic-volume stages normally own 0; terminal docking
        // owns 1; transition spans interpolate continuously.
        double dynamicRollPhaseWeight = 1.0;
    };

    struct Anchor
    {
        double progressMeters = 0.0;
        glm::dvec3 upReference {0.0, 1.0, 0.0};
        double dynamicRollPhaseWeight = 0.0;
    };

    // WORKING CONTRACT: canonical visual frame field is built from authored
    // routeCurves. With no semantic anchors this preserves the historical
    // terminal-dock-owned field. With anchors, orientation ownership becomes
    // piecewise while geometry remains one continuous route.
    [[nodiscard]] static std::vector<Sample> build(
        const std::vector<planner::RouteCurveSegment>& curves,
        const glm::dvec3& terminalUpReference,
        double canonicalStepMeters = 25.0
    )
    {
        return build(
            curves,
            terminalUpReference,
            std::vector<Anchor>{},
            canonicalStepMeters
        );
    }

    [[nodiscard]] static std::vector<Sample> build(
        const std::vector<planner::RouteCurveSegment>& curves,
        const glm::dvec3& terminalUpReference,
        const std::vector<Anchor>& requestedAnchors,
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
            const double finish = curve.endProgressMeters;
            if (!(finish >= begin))
                continue;

            appendGeometrySample(begin, curve);
            for (double s = begin + step;
                 s < finish - 1.0e-9;
                 s += step)
            {
                appendGeometrySample(s, curve);
            }
            appendGeometrySample(finish, curve);
        }

        if (out.empty())
            return out;

        struct ResolvedAnchor
        {
            std::size_t sampleIndex = 0;
            glm::dvec3 upReference {0.0, 1.0, 0.0};
            double dynamicRollPhaseWeight = 0.0;
        };

        std::vector<ResolvedAnchor> anchors;
        anchors.reserve(requestedAnchors.size() + 1);

        const auto nearestSampleIndex =
            [&](double progressMeters)
            {
                const double clamped = std::clamp(
                    progressMeters,
                    out.front().progressMeters,
                    out.back().progressMeters
                );
                const auto it = std::lower_bound(
                    out.begin(),
                    out.end(),
                    clamped,
                    [](const Sample& sample, double value)
                    {
                        return sample.progressMeters < value;
                    }
                );
                if (it == out.begin())
                    return std::size_t{0};
                if (it == out.end())
                    return out.size() - 1;

                const std::size_t hi =
                    static_cast<std::size_t>(it - out.begin());
                const std::size_t lo = hi - 1;
                return
                    std::abs(out[hi].progressMeters - clamped) <
                    std::abs(out[lo].progressMeters - clamped)
                        ? hi
                        : lo;
            };

        for (const auto& requested : requestedAnchors)
        {
            if (!std::isfinite(requested.progressMeters))
                continue;

            ResolvedAnchor anchor;
            anchor.sampleIndex =
                nearestSampleIndex(requested.progressMeters);
            anchor.upReference = requested.upReference;
            anchor.dynamicRollPhaseWeight = std::clamp(
                requested.dynamicRollPhaseWeight,
                0.0,
                1.0
            );
            anchors.push_back(anchor);
        }

        // Terminal docking frame remains the final authority.
        ResolvedAnchor terminal;
        terminal.sampleIndex = out.size() - 1;
        terminal.upReference = terminalUpReference;
        terminal.dynamicRollPhaseWeight = 1.0;
        anchors.push_back(terminal);

        std::sort(
            anchors.begin(),
            anchors.end(),
            [](const ResolvedAnchor& a, const ResolvedAnchor& b)
            {
                return a.sampleIndex < b.sampleIndex;
            }
        );

        // Same-index anchors: later semantic ownership wins. This makes an
        // explicit terminal anchor authoritative if a preceding stage ends
        // exactly on the route endpoint.
        std::vector<ResolvedAnchor> uniqueAnchors;
        for (const auto& anchor : anchors)
        {
            if (!uniqueAnchors.empty() &&
                uniqueAnchors.back().sampleIndex == anchor.sampleIndex)
            {
                uniqueAnchors.back() = anchor;
            }
            else
            {
                uniqueAnchors.push_back(anchor);
            }
        }
        anchors = std::move(uniqueAnchors);

        const auto normalizedUpAt =
            [&](std::size_t index, glm::dvec3 requested)
            {
                const glm::dvec3 forward = out[index].forward;
                requested -= forward * glm::dot(requested, forward);
                if (glm::length(requested) <= 1.0e-9)
                    requested = perpendicularSeed(forward);
                return glm::normalize(requested);
            };

        for (auto& anchor : anchors)
            anchor.upReference =
                normalizedUpAt(anchor.sampleIndex, anchor.upReference);

        // Before the first semantic owner, transport that owner's frame
        // backwards. Its dynamic phase ownership is held constant.
        {
            const auto& first = anchors.front();
            setFrame(out[first.sampleIndex], first.upReference);
            out[first.sampleIndex].dynamicRollPhaseWeight =
                first.dynamicRollPhaseWeight;

            for (std::size_t i = first.sampleIndex; i > 0; --i)
            {
                glm::dvec3 up = minimalRotate(
                    out[i].forward,
                    out[i - 1].forward,
                    out[i].up
                );
                up -= out[i - 1].forward *
                    glm::dot(up, out[i - 1].forward);
                if (glm::length(up) <= 1.0e-9)
                    up = perpendicularSeed(out[i - 1].forward);
                setFrame(out[i - 1], glm::normalize(up));
                out[i - 1].dynamicRollPhaseWeight =
                    first.dynamicRollPhaseWeight;
            }
        }

        // Between semantic owners, construct the roll interpolation from both
        // ends. Tangent geometry stays untouched; only rotation around the
        // route tangent is blended.
        for (std::size_t ai = 0; ai + 1 < anchors.size(); ++ai)
        {
            const auto& a = anchors[ai];
            const auto& b = anchors[ai + 1];

            setFrame(out[a.sampleIndex], a.upReference);
            setFrame(out[b.sampleIndex], b.upReference);
            out[a.sampleIndex].dynamicRollPhaseWeight =
                a.dynamicRollPhaseWeight;
            out[b.sampleIndex].dynamicRollPhaseWeight =
                b.dynamicRollPhaseWeight;

            if (b.sampleIndex <= a.sampleIndex + 1)
                continue;

            std::vector<glm::dvec3> leftUp(
                b.sampleIndex - a.sampleIndex + 1
            );
            std::vector<glm::dvec3> rightUp(
                b.sampleIndex - a.sampleIndex + 1
            );

            leftUp.front() = a.upReference;
            for (std::size_t i = a.sampleIndex + 1;
                 i <= b.sampleIndex;
                 ++i)
            {
                glm::dvec3 up = minimalRotate(
                    out[i - 1].forward,
                    out[i].forward,
                    leftUp[i - 1 - a.sampleIndex]
                );
                up -= out[i].forward * glm::dot(up, out[i].forward);
                if (glm::length(up) <= 1.0e-9)
                    up = perpendicularSeed(out[i].forward);
                leftUp[i - a.sampleIndex] = glm::normalize(up);
            }

            rightUp.back() = b.upReference;
            for (std::size_t i = b.sampleIndex;
                 i > a.sampleIndex;
                 --i)
            {
                glm::dvec3 up = minimalRotate(
                    out[i].forward,
                    out[i - 1].forward,
                    rightUp[i - a.sampleIndex]
                );
                up -= out[i - 1].forward *
                    glm::dot(up, out[i - 1].forward);
                if (glm::length(up) <= 1.0e-9)
                    up = perpendicularSeed(out[i - 1].forward);
                rightUp[i - 1 - a.sampleIndex] = glm::normalize(up);
            }

            const double s0 = out[a.sampleIndex].progressMeters;
            const double s1 = out[b.sampleIndex].progressMeters;
            const double span = std::max(1.0e-9, s1 - s0);

            for (std::size_t i = a.sampleIndex + 1;
                 i < b.sampleIndex;
                 ++i)
            {
                const double u = std::clamp(
                    (out[i].progressMeters - s0) / span,
                    0.0,
                    1.0
                );

                Sample left = out[i];
                setFrame(left, leftUp[i - a.sampleIndex]);
                Sample right = out[i];
                setFrame(right, rightUp[i - a.sampleIndex]);

                glm::dquat qa = left.orientation;
                glm::dquat qb = right.orientation;
                if (glm::dot(qa, qb) < 0.0)
                    qb = -qb;

                const glm::dquat q =
                    glm::normalize(glm::slerp(qa, qb, u));
                glm::dvec3 up =
                    q * glm::dvec3(0.0, 1.0, 0.0);
                up -= out[i].forward *
                    glm::dot(up, out[i].forward);
                if (glm::length(up) <= 1.0e-9)
                    up = perpendicularSeed(out[i].forward);
                setFrame(out[i], glm::normalize(up));

                out[i].dynamicRollPhaseWeight =
                    a.dynamicRollPhaseWeight * (1.0 - u) +
                    b.dynamicRollPhaseWeight * u;
            }
        }

        return out;
    }

    // Resolve the route progress of a semantic point. Exact Line matches are
    // preferred; curved primitives use a bounded nearest-point refinement.
    [[nodiscard]] static bool progressAtPoint(
        const std::vector<planner::RouteCurveSegment>& curves,
        const glm::dvec3& point,
        double& progressMeters
    )
    {
        bool found = false;
        double bestDistance2 =
            std::numeric_limits<double>::infinity();
        double bestProgress = 0.0;

        for (const auto& curve : curves)
        {
            const double length =
                std::max(
                    0.0,
                    curve.endProgressMeters -
                    curve.startProgressMeters
                );
            if (length <= 1.0e-12)
                continue;

            if (curve.kind == planner::RouteCurveKind::Line)
            {
                const glm::dvec3 delta =
                    curve.endMeters - curve.startMeters;
                const double delta2 = glm::dot(delta, delta);
                if (delta2 <= 1.0e-12)
                    continue;
                const double t = std::clamp(
                    glm::dot(point - curve.startMeters, delta) / delta2,
                    0.0,
                    1.0
                );
                const glm::dvec3 p =
                    curve.startMeters + delta * t;
                const glm::dvec3 error = point - p;
                const double d2 = glm::dot(error, error);
                if (d2 < bestDistance2)
                {
                    bestDistance2 = d2;
                    bestProgress =
                        curve.startProgressMeters + length * t;
                    found = true;
                }
                continue;
            }

            constexpr int CoarseSamples = 128;
            int bestI = 0;
            for (int i = 0; i <= CoarseSamples; ++i)
            {
                const double t =
                    static_cast<double>(i) /
                    static_cast<double>(CoarseSamples);
                const glm::dvec3 p =
                    curve.positionAtParameter(t);
                const glm::dvec3 error = point - p;
                const double d2 = glm::dot(error, error);
                if (d2 < bestDistance2)
                {
                    bestDistance2 = d2;
                    bestI = i;
                    bestProgress =
                        curve.startProgressMeters + length * t;
                    found = true;
                }
            }

            double lo = std::max(
                0.0,
                (static_cast<double>(bestI) - 1.0) /
                    static_cast<double>(CoarseSamples)
            );
            double hi = std::min(
                1.0,
                (static_cast<double>(bestI) + 1.0) /
                    static_cast<double>(CoarseSamples)
            );

            for (int iteration = 0; iteration < 24; ++iteration)
            {
                const double m1 = lo + (hi - lo) / 3.0;
                const double m2 = hi - (hi - lo) / 3.0;
                const glm::dvec3 e1 =
                    point - curve.positionAtParameter(m1);
                const glm::dvec3 e2 =
                    point - curve.positionAtParameter(m2);
                if (glm::dot(e1, e1) < glm::dot(e2, e2))
                    hi = m2;
                else
                    lo = m1;
            }

            const double t = 0.5 * (lo + hi);
            const glm::dvec3 error =
                point - curve.positionAtParameter(t);
            const double d2 = glm::dot(error, error);
            if (d2 < bestDistance2)
            {
                bestDistance2 = d2;
                bestProgress =
                    curve.startProgressMeters + length * t;
                found = true;
            }
        }

        if (!found)
            return false;

        progressMeters = bestProgress;
        return true;
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

    [[nodiscard]] static double dynamicRollPhaseWeightAtProgress(
        const std::vector<Sample>& field,
        double progressMeters
    )
    {
        if (field.empty())
            return 1.0;
        if (field.size() == 1 ||
            progressMeters <= field.front().progressMeters)
        {
            return field.front().dynamicRollPhaseWeight;
        }
        if (progressMeters >= field.back().progressMeters)
            return field.back().dynamicRollPhaseWeight;

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
        return std::clamp(
            field[lo].dynamicRollPhaseWeight * (1.0 - u) +
            field[hi].dynamicRollPhaseWeight * u,
            0.0,
            1.0
        );
    }

    [[nodiscard]] static std::vector<double>
    sampleDynamicRollPhaseWeightForGates(
        const std::vector<Sample>& field,
        const std::vector<planner::RouteGate>& gates
    )
    {
        std::vector<double> result;
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
            result.push_back(
                dynamicRollPhaseWeightAtProgress(
                    field,
                    estimatedProgress
                )
            );
        }
        return result;
    }

    // WORKING CONTRACT — DO NOT "SIMPLIFY" WITHOUT A REPRODUCING FAILURE.
    //
    // The visual docking tunnel is phase-locked to the rotating dock by TWO
    // geometric points in the same frame:
    //   1) aperture center;
    //   2) a radial reference point that means "dock bottom".
    //
    // The signed phase is measured around the TERMINAL ROUTE axis, not around
    // dock.forward.  Those axes are opposite on approach, so measuring around
    // dock.forward and then applying the angle around route.forward reverses
    // clockwise/counter-clockwise motion.
    //
    // This two-point contract is intentional.  It keeps dock and tunnel as one
    // kinematic object.  Do not replace it with an independent Euler angle,
    // arbitrary up-vector projection, or a separately integrated tunnel roll
    // unless there is a concrete bug that requires it.
    [[nodiscard]] static double synchronizedTunnelRollPhase(
        const glm::dvec3& terminalRouteAxisRequested,
        const glm::dvec3& planningDockCenter,
        const glm::dvec3& planningDockBottom,
        const glm::dvec3& liveDockCenter,
        const glm::dvec3& liveDockBottom
    )
    {
        return TwoPointRollGeometry::signedPhase(
            terminalRouteAxisRequested,
            {
                planningDockCenter,
                planningDockBottom
            },
            {
                liveDockCenter,
                liveDockBottom
            }
        );
    }

    // WORKING CONTRACT: dynamic dock roll changes frame orientation only
    // around each frame's own route tangent; centerline geometry is untouched.
    [[nodiscard]] static glm::dvec3 rotateUpAroundForward(
        const glm::dvec3& forwardRequested,
        const glm::dvec3& upRequested,
        double phaseRad
    )
    {
        const glm::dvec3 forward =
            normalizedOr(
                forwardRequested,
                glm::dvec3(0.0, 0.0, -1.0)
            );

        glm::dvec3 up =
            upRequested -
            forward * glm::dot(upRequested, forward);
        if (glm::length(up) <= 1.0e-9)
            up = perpendicularSeed(forward);
        up = glm::normalize(up);

        const glm::dvec3 rotated =
            glm::angleAxis(phaseRad, forward) * up;
        return glm::normalize(
            rotated -
            forward * glm::dot(rotated, forward)
        );
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
