# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted exact target-machine baseline

Exact tested checkout:

```
a0f0991791665e30059be15efc47dedcdfafe090
```

Target evidence:
- Stage-12 architecture contract: **PASS**.
- `navigation_runtime`: **17/17 PASS**.
- `maneuver_speed_doctrine_matrix`: PASS.
- previous maneuver/corridor/fly-through regressions remain green.

## Newly accepted: B7 speed/doctrine select -> execute gate

The same 180 m objective exposed six physical candidate programs:
- precision;
- balanced;
- fast;
- newtonian_drift_dash;
- low_threat_escape;
- reckless_shortcut.

Measured deterministic doctrine choices:

| Law | Rational | PrecisionRetrieval | Extreme | CombatEscape |
|---|---|---|---|---|
| Newtonian | balanced | precision | newtonian_drift_dash | low_threat_escape |
| Assisted | balanced | precision | fast | low_threat_escape |

The intentionally faster `reckless_shortcut` had criticalRisk=0.90 with preferred ceiling 0.20 and was rejected above doctrine.

## B7 measured execution results

All selected programs executed through:
`AcceptedManeuverProgram -> sampler/follower -> B10 -> PilotSkill -> SharedShipPhysics/DynamicMotionSystem`.

No selected row exceeded the tracking envelope.

Representative actual Expert results:

- **Rational / balanced**
  - time 24.0 s;
  - actual min clearance ~13.31 m;
  - peak speed ~9.75 m/s;
  - max slip ~1.77 deg.

- **PrecisionRetrieval / precision**
  - time 30.0 s;
  - actual min clearance ~22.30 m;
  - peak speed ~7.37 m/s;
  - max slip ~1.98 deg.

- **Extreme / Newtonian / newtonian_drift_dash**
  - time 18.0 s;
  - actual min clearance ~2.62 m;
  - peak speed ~14.11 m/s;
  - max slip ~34.08 deg;
  - final P error ~0.016 m;
  - final V error ~0.160 m/s.

- **Extreme / Assisted / fast**
  - time 20.0 s;
  - actual min clearance ~7.59 m;
  - peak speed ~12.28 m/s;
  - max slip ~2.08 deg.

- **CombatEscape / low_threat_escape**
  - time 22.5 s;
  - actual min clearance ~13.45 m;
  - peak speed ~10.81 m/s;
  - max slip ~1.93 deg.

This is the first direct proof that doctrine and control law can select physically different accepted programs and that the selected program remains executable.

## B7 status nuance

B7 **selection semantics + selected-program execution are accepted at lab/runtime level**.

The original production architecture note remains partly true:
ordinary live navigation still needs final wiring so its normal production chain consumes the B7-selected program instead of bypassing B7 through transitional compatibility seams.

Therefore:
- B7 algorithm/behavior gate: **closed**;
- B7 final ordinary-live production migration: **still integration work**.

## Canonical B0-B14 status after this acceptance

### Strong / accepted behavior
- B0 world snapshot/publication;
- B7 decision semantics and select->execute behavior;
- B8 AcceptedManeuverProgram;
- B9 sampler;
- B10 bounded tracking;
- B12 PilotSkill;
- B13 propulsion/physics;
- B14 scheduler.

### Strong mechanics, production generalization/integration still incomplete
- B5 physical compiler: Newtonian ordinary slice strong; full Assisted/general-family production coverage remains;
- B6 continuous proof: strong exact-static/moving proof, not yet one generalized ordinary block;
- B7/B8/B9/B10 ordinary-live wiring: final compatibility-seam retirement remains.

### Open/transitional
- B1 scene-wide sparse influence batching;
- B2 unified NavigationObjective;
- B3 vehicle/control-law-aware global edge feasibility;
- B4 route-aligned local corridor replacing ray-fan search;
- B11 explicit bounded safety-reflex API.

## Current testing stage

B7 speed/doctrine block is complete.

Next laboratory block:
**chained transitions + negative / physical-limit cases**.

It must prove the system behaves correctly when a feasible program changes family or when the requested maneuver is physically impossible.

Required scenarios:
1. moving fly-through -> hard turn -> braking/capture;
2. drift/high-slip -> aligned precision segment;
3. insufficient turn room;
4. insufficient braking distance;
5. corridor narrower than rigid hull;
6. incompatible control-law candidate set;
7. accepted program invalidated by newly introduced obstacle/change.

Correct negative result is not necessarily completion. It is:
- reject before ACCEPT;
- select another proved maneuver;
- reduce speed / brake / recover;
- or invalidate + replan fail-closed.

Never accept an unproved/impossible program merely to preserve progress.

## Documentation protocol

After every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 documentation;
- recreate `CONTINUE_PROMPT.md` **from scratch**.
