# CURRENT TASK — Replace point-mass corner timing with physically compiled maneuver geometry

Date: 2026-09-22

Status: **ARCHITECTURE FIX REQUIRED BEFORE MORE FOLLOWER TUNING**

Baseline before documentation commits:

```text
adfe567eefac994344d81c60b1f21e24f3d09077
```

## Finding

The current higher-speed failure is not just a bad Ruckig tuning constant.

Current multi-point chain:

```text
coarse collision-free polyline
    -> local Bezier corner rounding
    -> scalar path p(s)
    -> scalar Ruckig s(t)
    -> attitude authored afterwards
    -> follower tries to physically realize it
```

That ordering is backwards for a main-engine-dominant Newtonian craft.

Ruckig currently knows:
- scalar path distance;
- scalar path speed;
- one symmetric acceleration limit;
- jerk limit.

It does NOT know:
- hull attitude needed for the next burn;
- finite time needed to rotate the hull;
- aft-main direction;
- simultaneous RCS + main allocation;
- actual braking boundary including lead-rotation;
- whether the chosen curve is dynamically flyable by Cobra at that speed.

## Vehicle-data issue

The runtime harness uses a duplicated hard-coded `cobraParams()` instead of
the authoritative Cobra descriptor.

Current inspected values are duplicated correctly, but this is unsafe and must
be removed.

Worse, `maxLinearGs = 7.5` becomes ~73.55 m/s^2 forward/braking authority in
the trajectory profile. That is an envelope, not a complete propulsion model.

`turnRadius = 20 m` exists but is not the authoritative source for current
multi-point corner geometry.

## Required architecture

For each material maneuver/corner, compile a physically feasible primitive
before final timing:

```text
incoming state
(position, velocity, hull attitude, angular rate)

    -> choose geometric maneuver
       straight / arc / clothoid-like transition / lead-rotate+burn

    -> solve propulsion allocation
       aft main + RCS + angular authority

    -> compute lead-rotation start
       and braking/turn entry boundary

    -> produce target state samples / primitive constraints

    -> Ruckig times the already feasible scalar or state transition
       without inventing geometry or propulsion
```

At 30 m/s with free space, planner should be allowed to generate a broad arc
whose radius is determined by the actual maneuver envelope instead of retaining
an unnecessarily sharp polyline topology.

## Immediate implementation sequence

1. Eliminate duplicated `cobraParams()`; source the real descriptor/profile.
2. Separate **propulsion capability** from pilot/load envelope.
3. Add a maneuver-feasibility layer that computes:
   - available RCS vector;
   - aft-main contribution as a function of hull attitude;
   - angular rotation time;
   - braking/turn lead distance.
4. Upgrade corner geometry to use physically required radius/transition length.
5. Feed Ruckig physically feasible progress/state constraints after that.
6. Add a 30 m/s no-nearby-obstacle regression requiring a broad smooth arc and
   no corner overshoot.

Do not tune follower envelopes or add more recovery hacks until this planner /
maneuver-authoring boundary is corrected.
