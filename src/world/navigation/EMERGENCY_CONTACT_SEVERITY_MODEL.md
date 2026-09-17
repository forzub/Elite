# Navigation v2 — emergency contact severity model

**Status:** isolated candidate pending target-machine gate  
**Updated:** 2026-09-17 Europe/Kyiv  
**Stage:** `NAV-V2-TRAJECTORY-1`  
**Parent contracts:** `NAVIGATION_WORLD_V2.md`, `src/world/navigation/TRAJECTORY_CONTROL_MODEL.md`, `src/world/navigation/CONTINUOUS_PASSAGE_MODEL.md`

## Purpose

The accepted emergency invariant remains:

```text
no collision-free proof != no navigation command
```

The existing `EmergencyPassageMitigator` keeps a best-effort command alive when collision-free entry is already too late. The next question is narrower:

```text
if several bounded emergency trajectories all expect contact,
which predicted contact is physically less severe?
```

Implementation candidate:

```text
src/world/navigation/trajectory/EmergencyContactSeverityScorer.h
src/world/navigation/trajectory/EmergencyContactSeverityScorer.cpp
```

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

`v_surface` is already part of the witness so this same scorer can later rank contacts against moving obstacle gaps and moving/rotating docking geometry.

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

## Behavior fixtures

Target-machine runner:

```text
tests/navigation_trajectory/run_mingw64.sh
```

New fixture:

```text
navigation_trajectory_emergency_contact_severity
```

Pinned cases:

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

Do not add a dedicated benchmark before behavior acceptance. After the target-machine gate, measure only if live composition indicates this fixed-size scorer is material; current continuous eight-candidate verification already costs only `0.04135 ms p95`.

## Not yet claimed

This isolated scorer does not yet provide:

- prediction of exact contact time or contact point;
- continuous emergency trajectory synthesis toward the winning witness;
- reduced-mass calculation from both bodies' mass/inertia;
- material/restitution/friction response;
- vulnerable semantic hull-region weighting;
- moving-gap contact witness generation;
- moving/rotating docking witness generation.

Those belong to later trajectory/physics integration slices.
