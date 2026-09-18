# Elite — CURRENT STATE

**Updated:** 2026-09-18
**Canonical branch:** `main`
**Current public HEAD:** `e83a0a2bd5d54efb7737607c40ff789cafee5cf5`

## Stage 12 status

- 12A-1 — ACCEPTED
- 12A-2 — ACCEPTED
- 12A-3 — ACCEPTED
- 12A-4 exact static HitVolume OBB — ACCEPTED
- 12A-5 static/dynamic ownership cleanup — CANDIDATE

## Latest target-machine result

Run on `5aac072dbb13dbed755a4a9fe6dd1b91f24db717`:

```text
architecture PASS
EliteGame PASS
EliteServer PASS

first_live_probe_blocked=0
first_live_blocker_entity=0
obstacle_entity=22

first_live_horizon_m=1420.11
first_live_agent_map=(1014.47,-1767.96,-5679.82)
first_live_goal_map=(975,-1300,1000)
first_live_bounded_target_map=(1006.1,-1668.71,-4263.2)
```

Expected configured start was:

```text
(975,-1300,-6200)
```

So the first live ship state was already displaced by hundreds of metres before
planner composition.

## Root cause found

`placeShipInReferenceFrame()` reset legacy
`tr.localVelocity` but did not reset authoritative
`tr.motion.localVelocityMps`.

That allowed stale pre-placement local velocity to survive the frame transition.
On the next local-frame kinematic step, the ship moved away from the configured
start before its first navigation solve.

The same placement function also retained stale propulsion/alignment state.
The corrected contract now clears:
- localVelocityMps;
- mainEngineAccelerationMps2;
- manoeuvreAccelerationMps2;
- engineAccelerationMps2;
- desiredTacticalVelocityMps;
- velocityAlignmentMode;
- assisted target-speed hold state.

This is a production reference-frame placement bug, not a navigation-fixture
workaround.

Next gate must show first live agent position remaining on the configured start
line and CUBE 08 becoming the first exact blocker without reintroducing static
NavigationMap spheres.
