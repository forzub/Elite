#pragma once

// Numerical/topological tolerances for metre-valued navigation geometry.
// Do not round stored/planned coordinates; all geometry remains double.
//
// These values are NOT obstacle-clearance margins, hull-size allowances,
// corridor-following tolerances, or time/speed/angular thresholds.
// Physical feasibility and collision checks keep their own requirements.
namespace game::navigation::geometry_tolerance
{

// Separate calculations of the same authored waypoint/portal may differ by
// sub-millimetres after world/frame transforms and curve composition.
// Suppress tiny duplicate legs without quantizing actual coordinates.
inline constexpr double ConstructedPointMeters = 1.0e-3;

// A stationary craft can coast a few millimetres while an asynchronous
// planner works. Its first straight remains valid at essentially the same
// start; a genuinely earlier craft still needs the entry connector.
// This is an entry-adoption tolerance, NOT a collision/route-width allowance.
inline constexpr double EntryStartBackstepMeters = 1.0e-2;

// Likewise, planner completion can find the craft centimetres past the end
// of its first authored straight. This is not licence to skip a real turn.
inline constexpr double EntryEndOvershootMeters = 1.0e-2;

// Multi-leg semantic stage lengths add several independently calculated
// double-precision lengths; a centimetre mismatch is bookkeeping noise, not
// a missing tens/hundreds-of-metres mandatory straight.
inline constexpr double ComposedStageLengthMeters = 1.0e-2;

} // namespace game::navigation::geometry_tolerance
