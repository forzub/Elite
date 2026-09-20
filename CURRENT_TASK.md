# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

## Latest actually tested checkout

```
2ad1178bc5c778636748557ceb6c9a5b757c9a53
```

## Current unverified mechanism baseline before state-sync commits

```
edb4c4106686ce625e1cd5eb99a6d1483cd32854
```

## Task

Run the first target-machine compile/behavior gate for the hard-replaced B4
visible-horizon solver.

The mechanism being tested is:

```text
nominal trajectory
 -> visible horizon
 -> predicted dynamic occupancy
 -> normal-plane projection
 -> metric offset grid
 -> longitudinal bypass station
 -> exact-static proof of outbound leg
 -> exact-static proof of return/merge leg
 -> time-coupled dynamic proof of complete detour
 -> temporary bypass
 -> on-route merge
```

## What must not happen

Do not restore:
- angular fan;
- deflection rings;
- azimuth branches;
- branch continuity;
- branch-switch Brake/recovery.

Do not weaken clearance just to get green.

## First-run priorities

If build fails:
- fix exact API/include/type error only.

If focused tests fail:
- inspect projection working set;
- basis construction;
- metric step size;
- longitudinal station search;
- first/second exact-static segment evidence;
- time-coupled dynamic proof;
- fixture geometry.

If composite fails:
- inspect each projected bypass target, forward station, merge point, planned
  and actual clearance;
- keep planner/executor world truth synchronized.

## Exit criterion

Only a fresh target-machine green gate can promote the replacement.

If green:
- record exact tested HEAD and metrics;
- synchronize all required MDs;
- recreate `CONTINUE_PROMPT.md` from scratch;
- then move toward actual NAV STRESS/game visual evaluation rather than adding
  unrelated synthetic matrices.
