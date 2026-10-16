# ASSISTED PRE-PLANNING ENTRY CONTRACT

Status: **HARD / AUTHORITATIVE ASSISTED CONTRACT**

This document defines the mandatory moving-entry algorithm for Assisted
navigation and docking. Tests and implementation must conform to this sequence.
A unit test is not allowed to redefine these semantics.

## 1. Initial moving state

The algorithm applies for **any finite initial translational speed**.

The navigation request must not invent a special low-speed entry mode merely to
make planning easier.

At request start:

1. clear all commanded linear acceleration;
2. damp commanded/actual angular motion to zero;
3. preserve the current translational velocity;
4. once angular motion is settled, the ship continues as a ballistic coast:
   constant velocity, straight line, zero commanded acceleration.

The initial velocity vector is authoritative. Hull nose is not the navigation
course.

## 2. Ballistic course safety check

While the ship is coasting, test the current velocity course against obstacles
using the accepted spatial footprint/tube test.

If the ballistic course is clear:

- do not turn the ship merely for planning convenience;
- keep zero commanded acceleration;
- keep zero angular velocity;
- continue uniform rectilinear coast.

If the ballistic course is blocked:

1. select a new physically safe Assisted course using the spatial safety test;
2. perform exactly the required physical course-change maneuver using ordinary
   Assisted controls;
3. once the new velocity course is acquired, clear linear acceleration again;
4. damp angular velocity to zero again;
5. resume uniform rectilinear ballistic coast on the new course.

The safe-course maneuver is a **one-time course acquisition**, not a continuously
moving route-following arc and not an alternate route executor.

For a static request, one primary course correction is expected unless a genuinely
new runtime hazard appears.

## 3. Planning while the ship coasts

Expensive route planning/compilation runs asynchronously while the ship is in
the zero-acceleration ballistic coast state.

The planner must account for the fact that the ship continues moving during the
calculation. It must not freeze the ship at the request-time position and it must
not require the ship to brake to zero in Automatic mode.

The route backbone is planned first from the downstream/second fixed route point
toward the destination/start-of-final-routing boundary as required by the
current route topology.

The planner records the real calculation duration for this work.

## 4. Compute the actual route start point

Using the measured planning duration and the ship's current ballistic velocity,
compute the distance the ship will travel before the authored route can actually
begin.

From that travel distance, determine the real route start point on the current
ballistic course.

This start point is therefore time-dependent but is derived from:

- measured/accepted planning duration;
- current ballistic velocity;
- the already planned downstream route geometry.

It is not derived by continuously steering the ship toward a moving planning
reference.

## 5. Recompute the final entry segment

After the actual start point is known:

1. preserve the expensive downstream route/backbone;
2. recompute only the entry/last connector from the calculated start point to
   the already accepted second route point;
3. validate that connector physically;
4. if necessary, project/rebind the cheap entry connector again using the latest
   ballistic position without rebuilding the expensive route.

This is the accepted late-entry rebound/rebind mechanism.

A rebind is not a new docking request and must not re-run the primary safe-course
selector unless a genuinely new obstacle invalidates the current ballistic
course.

## 6. Control-state invariant before route execution

Immediately before handing the authored route to the follower:

- the ship has a finite translational velocity;
- commanded linear acceleration is zero;
- angular velocity is settled to zero/tolerance;
- motion is ballistic and straight;
- the planned entry point corresponds to where that ballistic state can
  physically meet the route.

Only after route execution begins may the follower author/control the normal
Assisted acceleration needed to follow route curvature and speed profile.

## 7. Forbidden substitutions

The following are contract violations:

- stopping the ship merely because the planner needs time;
- continuously steering along a "safe-course recovery arc" while expensive
  route planning runs;
- treating hull nose rotation as route progress;
- using wall-clock time itself as completed physical maneuver progress;
- using ballistic forward travel as proof that a course-change maneuver was
  physically completed;
- continuously chasing a moving reference before route execution is accepted;
- rebuilding the expensive route solely because the ship coasted during
  calculation;
- re-running primary safe-course selection for an ordinary late-entry rebind;
- changing this sequence only to make an isolated unit test green.

## 8. Manual mode difference

Manual guidance consumes the same route topology and the same planner geometry.

The intentional difference is only the initial state:

- Automatic: preserve translational velocity and coast ballistically while
  planning;
- Manual: virtual pilot brakes translational velocity to zero, damps angular
  velocity, then runs the same planner from the stopped state.

## 9. Required regression structure

The contract must be tested at three levels:

1. **Pure state-machine test**
   - acceleration cleared;
   - angular velocity settled;
   - ballistic coast preserved;
   - safe-course correction occurs only if blocked;
   - after correction, coast is restored.

2. **Planner timing/rebind test**
   - planning duration produces the route-start distance;
   - only the entry connector is recomputed;
   - expensive backbone remains unchanged.

3. **Live physical integration test**
   - Assisted-I/Y/X;
   - non-zero high-speed initial velocity;
   - optional large hull/velocity angle;
   - no obstacle: coast remains straight;
   - obstacle: one physical safe-course correction, then straight coast;
   - planner completes while coasting;
   - route handoff occurs from the computed moving start point;
   - follower then executes the accepted route.

A green pure unit test is insufficient if the live sequence above is violated.
