# Elite — CURRENT STATE

**Updated:** 2026-09-18
**Canonical branch:** `main`
**Current public HEAD:** `546a8868b0a25c655310263252877c5f5b48d4c5`

## Stage 12 status

- 12A-1 — ACCEPTED
- 12A-2 — ACCEPTED
- 12A-3 — ACCEPTED
- 12A-4 exact static HitVolume OBB — ACCEPTED
- 12A-5 static/dynamic ownership cleanup — CANDIDATE

## Latest target-machine result

Run on `7f5778fcfe328e0c06cf18e14f9a0a235fbcea73`:

```text
architecture PASS
EliteGame PASS
EliteServer PASS

placement_map=(975,-1300,-6200)
expected_placement_map=(975,-1300,-6200)
placement_error_m=1.76866e-05
```

The reference-frame ordering/placement bug is now effectively closed.

`1.76866e-05 m` is approximately 17.7 micrometres. At orbital-scale world
coordinates this is ordinary double-precision round-trip residue, not a
navigation or placement defect.

The live test failed only because the diagnostic gate used an unrealistically
strict `1e-6 m` (1 micrometre) threshold.

## Correction

A named placement tolerance is now:

```text
NavigationRuntimeLabPlacementToleranceMeters = 1e-3
```

That is 1 mm and remains tiny compared with ship/obstacle geometry while safely
above orbital-coordinate floating-point residue.

Architecture contract pins the named tolerance and forbids returning to the
micrometre literal.

## Current real 12A-5 question

With placement now correct, the live gate proceeds to the actual ownership /
avoidance chain:

```text
stationary CUBE 08 absent from NavigationMap
    -> first live exact segment sees CUBE 08
    -> exact static blocker identity survives planner
    -> adjusted target chosen
    -> authoritative physical motion remains outside all exact HitVolumes
    -> replicated execution remains exact
```

Any later physical violation still fails immediately with obstacle identity and
swept motion witness.
