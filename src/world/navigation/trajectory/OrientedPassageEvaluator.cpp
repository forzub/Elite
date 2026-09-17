#include "OrientedPassageEvaluator.h"

#include <algorithm>
#include <cmath>

namespace world::navigation
{
namespace
{

using Evaluator = OrientedPassageEvaluator;
using Vec3d = Evaluator::Vec3d;
using Basis3d = Evaluator::Basis3d;

constexpr double kEpsilon = 1.0e-12;
constexpr double kBasisTolerance = 1.0e-6;

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite(const Vec3d& value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

Vec3d subtract(const Vec3d& a, const Vec3d& b) noexcept
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3d scale(const Vec3d& value, double scalar) noexcept
{
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

double dot(const Vec3d& a, const Vec3d& b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3d cross(const Vec3d& a, const Vec3d& b) noexcept
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

double lengthSquared(const Vec3d& value) noexcept
{
    return dot(value, value);
}

double length(const Vec3d& value) noexcept
{
    return std::sqrt(lengthSquared(value));
}

Vec3d normalizeOrZero(const Vec3d& value) noexcept
{
    const double magnitude = length(value);
    if (!finite(magnitude) || magnitude <= kEpsilon)
        return {};
    return scale(value, 1.0 / magnitude);
}

bool unitVector(const Vec3d& value) noexcept
{
    return finite(value) && std::abs(lengthSquared(value) - 1.0) <= kBasisTolerance;
}

bool validBasis(const Basis3d& basis) noexcept
{
    return unitVector(basis.right) &&
        unitVector(basis.up) &&
        unitVector(basis.forward) &&
        std::abs(dot(basis.right, basis.up)) <= kBasisTolerance &&
        std::abs(dot(basis.right, basis.forward)) <= kBasisTolerance &&
        std::abs(dot(basis.up, basis.forward)) <= kBasisTolerance;
}

bool validHull(const Evaluator::HullProxy& hull) noexcept
{
    return finite(hull.halfExtentsBodyMeters) &&
        hull.halfExtentsBodyMeters.x >= 0.0 &&
        hull.halfExtentsBodyMeters.y >= 0.0 &&
        hull.halfExtentsBodyMeters.z >= 0.0 &&
        finite(hull.additionalClearanceMeters) &&
        hull.additionalClearanceMeters >= 0.0;
}

bool validPassage(const Evaluator::Passage& passage) noexcept
{
    return finite(passage.centerMapMeters) &&
        validBasis(passage.passageToMap) &&
        finite(passage.halfWidthMeters) &&
        finite(passage.halfHeightMeters) &&
        passage.halfWidthMeters > 0.0 &&
        passage.halfHeightMeters > 0.0;
}

double projectedHalfExtent(
    const Evaluator::HullProxy& hull,
    const Basis3d& body,
    const Vec3d& axis
) noexcept
{
    return
        std::abs(dot(body.right, axis)) * hull.halfExtentsBodyMeters.x +
        std::abs(dot(body.up, axis)) * hull.halfExtentsBodyMeters.y +
        std::abs(dot(body.forward, axis)) * hull.halfExtentsBodyMeters.z +
        hull.additionalClearanceMeters;
}

} // namespace

OrientedPassageEvaluator::Passage
OrientedPassageEvaluator::makeObstacleGapPassage(
    const ObstacleGap& gap
) noexcept
{
    Passage passage;
    passage.source = PassageSource::ObstacleGap;
    passage.centerMapMeters = gap.centerMapMeters;
    passage.halfWidthMeters = 0.5 * gap.clearSeparationMeters;
    passage.halfHeightMeters = 0.5 * gap.secondaryClearanceMeters;

    if (!finite(gap.centerMapMeters) ||
        !finite(gap.travelDirectionMap) ||
        !finite(gap.separationAxisMap) ||
        !finite(gap.clearSeparationMeters) ||
        !finite(gap.secondaryClearanceMeters) ||
        gap.clearSeparationMeters <= 0.0 ||
        gap.secondaryClearanceMeters <= 0.0)
    {
        passage.passageToMap = {{}, {}, {}};
        return passage;
    }

    const Vec3d forward = normalizeOrZero(gap.travelDirectionMap);
    const Vec3d separationProjected = subtract(
        gap.separationAxisMap,
        scale(forward, dot(gap.separationAxisMap, forward))
    );
    const Vec3d right = normalizeOrZero(separationProjected);
    const Vec3d up = normalizeOrZero(cross(forward, right));

    passage.passageToMap.right = right;
    passage.passageToMap.up = up;
    passage.passageToMap.forward = forward;
    return passage;
}

OrientedPassageEvaluator::Result OrientedPassageEvaluator::evaluate(
    const HullProxy& hull,
    const Pose& pose,
    const Passage& passage
) noexcept
{
    Result result;

    if (!validHull(hull) ||
        !finite(pose.centerMapMeters) ||
        !validBasis(pose.bodyToMap) ||
        !validPassage(passage))
    {
        result.status = Status::InvalidInput;
        return result;
    }

    const Vec3d relative = subtract(pose.centerMapMeters, passage.centerMapMeters);
    result.lateralOffsetMeters = dot(relative, passage.passageToMap.right);
    result.verticalOffsetMeters = dot(relative, passage.passageToMap.up);

    result.projectedHalfWidthMeters = projectedHalfExtent(
        hull,
        pose.bodyToMap,
        passage.passageToMap.right
    );
    result.projectedHalfHeightMeters = projectedHalfExtent(
        hull,
        pose.bodyToMap,
        passage.passageToMap.up
    );

    result.widthClearanceMeters = passage.halfWidthMeters -
        (std::abs(result.lateralOffsetMeters) + result.projectedHalfWidthMeters);
    result.heightClearanceMeters = passage.halfHeightMeters -
        (std::abs(result.verticalOffsetMeters) + result.projectedHalfHeightMeters);

    const double conservativeRadius =
        std::sqrt(
            hull.halfExtentsBodyMeters.x * hull.halfExtentsBodyMeters.x +
            hull.halfExtentsBodyMeters.y * hull.halfExtentsBodyMeters.y +
            hull.halfExtentsBodyMeters.z * hull.halfExtentsBodyMeters.z
        ) + hull.additionalClearanceMeters;
    result.conservativeSphereFits =
        std::abs(result.lateralOffsetMeters) + conservativeRadius <=
            passage.halfWidthMeters &&
        std::abs(result.verticalOffsetMeters) + conservativeRadius <=
            passage.halfHeightMeters;

    if (result.projectedHalfWidthMeters > passage.halfWidthMeters)
    {
        result.status = Status::TooWide;
        return result;
    }
    if (result.projectedHalfHeightMeters > passage.halfHeightMeters)
    {
        result.status = Status::TooTall;
        return result;
    }
    if (result.widthClearanceMeters < 0.0 || result.heightClearanceMeters < 0.0)
    {
        result.status = Status::OffsetOutside;
        return result;
    }

    result.status = Status::Fits;
    result.fits = true;
    return result;
}

} // namespace world::navigation
