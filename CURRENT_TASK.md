# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-LOCAL-1` — compact-candidate scaling accepted; conservative lateral avoidance behavior gate pending

## Closed local reference gates

`LocalHorizonPlanner` behavior is accepted:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
navigation_local: PASS
```

Target-machine compact-candidate scaling on `4b94048b15e6e2cd32754b6b8d48daedcb18625f` is also accepted:

```text
scenario         p95_us     p95_ns/candidate
clear_16          0.4814          30.0873
clear_64          1.8327          28.6362
clear_256         7.9820          31.1798
clear_1024       36.9641          36.0977

conflict_16       0.5073          31.7062
conflict_64       1.9333          30.2078
conflict_256      7.5441          29.4693
conflict_1024    31.8656          31.1188

stale_1024        0.0359          0 candidates examined
```

Decision: the reference candidate loop is not the bottleneck. Do not optimize it further without new runtime evidence.

## Active candidate now on `main`

New backend-neutral block inside the accepted local layer:

```text
src/world/navigation/local/
    LocalAvoidancePlanner.h
    LocalAvoidancePlanner.cpp

tests/navigation_local/
    NavigationLocalAvoidanceTests.cpp

tests/architecture_contracts/
    check_navigation_local_avoidance.py
```

`LocalAvoidancePlanner` composes, but does not replace, the accepted `LocalHorizonPlanner`.

### Reference algorithm

When the nominal local result is already `Clear`, it is returned unchanged with zero avoidance probes. `StaleHold` also exits before probes.

For `ConflictHold`:

```text
NavigationSpace::queryPoint(agent)
        |
        v
3D deterministic lateral fan
    15 deg x 8 azimuths
    30 deg x 8 azimuths
        |
        v
NavigationSpace::queryPoint(candidate target)
        |
        +-- different/non-traversable region -> reject
        |
        v
same-region static proof
        |
        v
LocalHorizonPlanner dynamic recheck
        |
        +-- first Clear -> AdjustedClear / PassThrough
        +-- none Clear  -> ConflictHold
```

The same-region condition is deliberate: a semantic NavigationSpace region is a convex AABB. Two envelope-safe endpoints in the same region prove the straight segment remains inside that static free-space volume.

### Important safety limit

The accepted closest-approach reference uses the ship's **current** P/V/A. Therefore a lateral target may clear a future swept-corridor blocker, but it may not erase a currently predicted head-on/crossing conflict. Such conflicts remain fail-closed `ConflictHold` until a later trajectory-aware maneuver is separately demonstrated.

No portal-crossing bypass is accepted yet. A valid portal maneuver may be conservatively rejected.

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_local_boundary.py
python tests/architecture_contracts/check_navigation_local_avoidance.py
bash tests/navigation_local/run_mingw64.sh
```

Send the complete output.

## Decision after behavior gate

If architecture + both local test executables PASS:

1. accept the same-region avoidance behavior slice;
2. add a dedicated avoidance benchmark that measures multiplied probe work separately from the already accepted one-pass reference;
3. measure nominal-clear, early-adjust, all-static-rejected and all-dynamic-rejected cases on the target machine;
4. only then decide whether the 16-probe fan is cheap enough as-is or needs probe ordering/candidate budgeting changes;
5. after that, design the trajectory-aware path for head-on/crossing conflicts.

Do not wire live `EliteGame` / `EliteServer`, add pursuit-specific intercept logic, or claim head-on avoidance before these gates pass.
