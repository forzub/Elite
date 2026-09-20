# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

## Mandatory workflow

At the beginning of the dialog read the exact current `main` versions of:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- relevant production/tests for the active failure.

After every state-affecting event synchronize those state files and Stage-12,
then recreate this entire `CONTINUE_PROMPT.md` **from scratch from current
truth**. Never incrementally preserve stale prompt prose.

## Evidence boundary

Last accepted target-machine baseline:
```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Latest target-tested checkout:
```
81d0c23ae42bba0352026d6c2306cc6976c04bda
```

Latest user target result:
- Stage-12 architecture contract PASS;
- build/link PASS;
- 17/19 runtime tests PASS (89%);
- `navigation_runtime_planner` FAIL:
  `dynamic blocker must produce a local visibility bypass`;
- `navigation_composite_proving_ground` FAIL:
  first Newtonian local replan returned `ConflictHold`,
  `localBypassExhausted=1` after 168 offset candidates and 168 route candidates.

Do not call the hard B4 replacement accepted.

## Canonical B4 architecture

Ordinary unexpected-obstacle handling is only:

```text
accepted trajectory
 -> physical visible horizon
 -> predicted dynamic occupancy
 -> project into plane normal to trajectory
 -> bounded metric lateral/vertical offset search
 -> bounded longitudinal bypass-station search
 -> exact-static proof current->bypass
 -> exact-static proof bypass->merge
 -> time-coupled dynamic proof of complete detour
 -> publish bypass target + on-route merge target
 -> B5/B6 physical authoring/proof
 -> execute
 -> reacquire accepted trajectory
```

Do not restore angular fans, deflection rings, azimuth branches, branch
continuity hints, branch-switch API, or mandatory Brake-before-side-change.

## Proven issue in the focused failing test

`testAdjustedVisibilityDoesNotInheritFuturePortalAlignment()` uses:

```text
start X      = 2.0
blocker X    = 4.5
merge/stage X= 7.0
agent radius = 1.0
blocker radius = 0.75
safety margin = 0.0
projection padding = 1.0
```

Required dynamic separation = 2.75 m.
Distance blocker->start = 2.5 m.
Distance blocker->merge = 2.5 m.

`timeCoupledBypassClear()` samples alpha 0..1 inclusive and applies the same
required separation. Therefore `AdjustedClear` is impossible under this
fixture. Correct fixture geometry; do not weaken global clearance.

## Proven geometry in the composite failure

Logged row:

```text
position=(138.841366,52.623525,0)
hazard=(185.850434,42.920559,0)
merge_target=(168.222033,46.559171,0)
nominal_dynamic_conflicts=1
projected_obstacles=1
offset_candidates=168
route_candidates=168
bypass_exhausted=1
nominal_static_blocked=0
```

Distances:
- position->hazard = 48.0 m;
- position->merge = 30.0 m;
- merge->hazard = 18.0 m.

Composite dynamic separation requirement:

```text
sqrt(13^2 + 2.5^2 + 11.1^2) = 17.275995 m hull radius
+ 6.0 m hazard radius
+ 2.0 m safety margin
+ 1.5 m projection padding
= 26.775995 m
```

At t=4 s the hazard has shifted only 2 m in Y, so merge->hazard is still
about 18.51 m. Because the final sample of every candidate is the same merge
point, every candidate necessarily fails the full dynamic proof.

## What remains uncertain

The focused failure is a stale fixture. The composite also exposes a possible
production limitation: `LocalAvoidancePlanner` fixes merge to
`boundedNominalTarget`. If dynamic occupancy covers that point, the solver
cannot remain safely off-route and merge later even when such a continuation
may exist.

Do not guess whether the correct fix is test-only or production-only.

## Next work

1. Add initial failure diagnostics for `projectionRejected`, `staticRejected`,
   and `dynamicRejected`.
2. Repair the focused portal-attitude fixture by changing geometry, not
   clearance.
3. Add a deterministic regression where the first bounded merge is inside the
   dynamic exclusion envelope but a safe later reacquisition exists.
4. Decide the B4 rule:
   - sample downstream merge stations, or
   - enlarge/adapt the physical horizon, or
   - carry a proved off-route continuation across receding horizons and merge
     later.
5. Preserve fail-closed behavior if no bounded safe continuation is proved.
6. Re-run architecture + full 19-test MinGW64 runtime gate.

## Non-negotiable ownership

- Planner owns route/corridor geometry and proof.
- Follower tracks an accepted program; it does not invent a maneuver.
- Stable automatic execution does not replan every frame.
- Static and dynamic obstacle ownership remain separate.
- Safety margins/radii must not be reduced merely to turn the suite green.
