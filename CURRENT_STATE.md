# Elite — CURRENT STATE

**Updated:** 2026-09-18
**Canonical branch:** `main`

## Stage 12

- 12A-1 — ACCEPTED
- 12A-2 — ACCEPTED
- 12A-3 — ACCEPTED
- 12A-4 exact static HitVolume OBB — ACCEPTED
- 12A-5 static/dynamic ownership cleanup — CANDIDATE

## Latest target-machine evidence

Run on `35802cec694b1c68fa07736596a38bb44d708375`:

```text
architecture PASS
navigation_runtime 3/3 PASS
EliteGame PASS
EliteServer PASS

obstacle_candidate=0
obstacle_conflict=0

configured_route_exact_block=1
first_live_probe_blocked=1
exact_obstacle_block=1
exact_static_block=1
adjusted=1

exact_static_motion_samples=6000
exact_static_violation=1
```

This is progress, not a repeat.

The previous failure was "live planner does not see CUBE 08".
That is now closed: the first live bounded segment, runtime planner and exact
static blocker identity all agree and the planner chooses an adjusted target.

The remaining failure is downstream:
the authoritative physical ship intersects some exact static HitVolume while
executing the accepted adjusted maneuver.

Therefore current debugging focus is no longer NavigationSpace ownership or
static detection. It is the boundary:

```text
geometrically safe adjusted target
    -> PilotSkillExecutor / control
    -> physically executed trajectory
```

A first-collision witness is now captured and the self-test fails immediately
at the first physical exact-static violation, reporting:
- blocking entity id;
- proving CUBE 08 entity id;
- swept start/end positions;
- selected target;
- planner status.

No stationary conservative sphere has been reintroduced.
