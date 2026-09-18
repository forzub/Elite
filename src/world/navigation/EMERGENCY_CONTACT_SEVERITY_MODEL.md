# Navigation v2 — emergency contact severity model

**Status:** architecture/behavior accepted on target machine  
**Updated:** 2026-09-17 Europe/Kyiv  
**Stage:** `NAV-V2-TRAJECTORY-1`  
**Parent contracts:** `NAVIGATION_WORLD_V2.md`, `src/world/navigation/TRAJECTORY_CONTROL_MODEL.md`, `src/world/navigation/CONTINUOUS_PASSAGE_MODEL.md`

## Purpose

The accepted emergency invariant remains:

```text
no collision-free proof != no navigation command
```

The existing `EmergencyPassageMitigator` keeps a best-effort command alive when collision-free entry is already too late. `EmergencyContactSeverityScorer` answers the narrower question:

```text
if several bounded emergency trajectories all expect contact,
which predicted contact is physically less severe?
```

Accepted implementation:

```text
src/world/navigation/trajectory/EmergencyContactSeverityScorer.h
src/world/navigation/trajectory/EmergencyContactSeverityScorer.cpp
```

## Target-machine acceptance

Accepted on canonical public commit:

```text
7f1bccd4e8b91c72e4fc5f9e6d1329260790aa8e
```

Architecture gate:

```text
NAVIGATION TRAJECTORY EMERGENCY CONTACT SEVERITY CONTRACT: PASS
```

Full trajectory suite:

```text
navigation_trajectory_passage                       PASS
navigation_trajectory_gap                           PASS
navigation_trajectory_reachability                  PASS
navigation_trajectory_emergency_passage             PASS
navigation_trajectory_continuous_passage            PASS
navigation_trajectory_emergency_contact_severity    PASS

100% tests passed, 0 failed out of 6
Total Test time: 0.27 sec
```

Decision: **behavior/architecture accepted**. No separate microbenchmark is required for this fixed-size scorer unless later live composition shows material cost.

## Ownership boundary

The scorer does **not** discover collisions.

It consumes already-predicted contact witnesses from a bounded trajectory/CCD preparation boundary and ranks at most:

```text
8 emergency trajectory candidates
4 contact witnesses per candidate
```

It owns no world search, spatial index, `NavigationMap`, `NavigationSpace`, renderer, OpenGL, or scene traversal.

Exact collision truth remains downstream:

```text
Navigation / trajectory
    predicted witness + least-severity emergency intent

Physics / Collision
    exact narrow phase / CCD / TOI / manifold / impulse / ricochet

Damage / Structural
    damage / breach / detach / destruction
```

A scorer result never fabricates a safe post-impact state.

## Contact-point velocity

Emergency severity uses velocity at the predicted body contact point, not only center-of-mass velocity.

For lever arm:

```text
r = contactPoint - shipCenter
```

rigid-body point velocity is:

```text
v_ship_contact = v_center + omega x r
```

Relative contact velocity is:

```text
v_rel = v_ship_contact - v_surface
```

`v_surface` is part of the witness, so this same accepted scorer can rank contacts against moving obstacle gaps and moving/rotating geometry once those witnesses are produced.

## Contact normal convention

Each witness supplies a normal pointing:

```text
obstacle/contact surface -> free space
```

The scorer normalizes it internally.

Closing normal speed is:

```text
v_n = max(0, -dot(v_rel, n))
```

Interpretation:

```text
v_n = 0
    tangent or separating at the witness instant

small v_n
    glancing contact / ricochet-friendly geometry

large v_n
    increasingly normal / severe impact
```

This is the primary emergency ranking metric.

## Coarse impact proxies

The scorer also exposes:

```text
normal momentum proxy = m_eff * v_n
normal energy proxy   = 0.5 * m_eff * v_n^2
```

`m_eff` may be supplied by a later physics-aware predictor. If no effective impact mass is supplied, the controlled ship mass is used as a coarse proxy.

These values are **ranking diagnostics**, not exact collision impulse or damage energy. Exact rotational inertia/contact Jacobian/material response remain physics authority.

Incidence angle is measured from the tangent plane:

```text
0      = perfectly tangential/glancing
pi / 2 = purely normal impact
```

## Deterministic priority

Candidate selection is lexicographic and bounded:

```text
1. no predicted contact beats predicted contact
2. lower peak closing normal speed
3. lower total normal-impact energy proxy
4. lower total normal momentum proxy
5. lower worst incidence angle from tangent
6. lower existing geometry deficit
7. higher useful passage-axis progress
8. lower stable candidate id / input index
```

Impact severity always outranks route progress. The system must never choose a more normal impact merely because that trajectory advances farther through the gap.

## Why peak normal speed is first

Summed energy alone can hide one catastrophic contact among several mild contacts. The first safety ordering therefore minimizes the worst predicted normal closing speed before considering summed energy.

This is intentionally conservative. A later structural damage model may refine effective mass and vulnerable contact regions without changing the navigation/physics ownership boundary.

## Accepted behavior fixtures

```text
high-total-speed glancing contact
    beats lower-total-speed normal impact when v_n is smaller

omega x r contact-point motion
    contributes to predicted normal impact speed

moving surface
    relative velocity uses ship point velocity - surface velocity

same normal speed / different effective mass
    lower energy proxy wins the tie

no-contact candidate
    always beats an emergency contact candidate

pure normal impact
    reports pi/2 incidence from tangent

invalid zero normal / >8 candidates
    fail closed
```

Architecture checker:

```text
tests/architecture_contracts/check_navigation_trajectory_emergency_contact_severity.py
```

## Performance

This scorer is intentionally tiny and fixed-size:

```text
<= 8 candidates
<= 4 witnesses each
no heap allocation required by the scorer
no world scan
```

The accepted continuous eight-candidate verification already costs only `0.04135 ms p95`; do not introduce a dedicated scorer benchmark unless runtime composition later shows this scorer is material.

## Next dependency

The scorer is now waiting on time-varying geometry/witness production rather than more scoring math.

The active next slice is:

```text
MovingGapPredictor
```

which predicts one already-selected obstacle pair over a short horizon, publishes moving gap center/boundary kinematics, and continuously proves that the gap does not close or rotate into a longitudinal/non-passage configuration between time samples.

## Still not claimed

The accepted scorer itself does not provide:

- prediction of exact ship contact time or ship hull contact point;
- continuous emergency trajectory synthesis toward the winning witness;
- reduced-mass calculation from both bodies' mass/inertia;
- material/restitution/friction response;
- vulnerable semantic hull-region weighting;
- moving/rotating docking witness generation.

Those belong to later trajectory/physics integration slices.


## Maneuver-decision ownership refinement — 2026-09-18

The accepted emergency invariant is now promoted into the game-level maneuver
decision tree:

```text
no collision-free proof != no navigation command
no global route != automatic hold
```

Authority document:

```text
src/game/MANEUVER_DECISION_TREE.md
```

`EmergencyContactSeverityScorer` remains deliberately physical. It ranks
contact severity (normal closing speed, energy/momentum proxies, incidence,
geometry deficit and progress), but it must not assign semantic value to the
contacted ship component.

A higher `ManeuverDecisionController` consumes additional annotations from
damage/structural and threat systems:

```text
criticalDamageRisk
missionDamageCost
expendableDamageCost
threatExposure / projected silhouette
escape reserve / returnability
time and exit speed
```

This separation is required for decisions such as:
- sacrifice radar/antenna rather than cockpit/reactor;
- preserve speed in Extreme doctrine;
- brake to create margin in Rational doctrine;
- minimize projected silhouette toward a pursuer in CombatEscape;
- enter wreckage slowly in Precision/Retrieval even though the same aperture
  might be taken violently during an escape.

The navigation/trajectory layer supplies candidates and witnesses. The
game/control decision layer selects doctrine and candidate. Physics/damage remain
final authority after actual contact.
