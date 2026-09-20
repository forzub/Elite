# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Last accepted exact target-machine baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Accepted:
- Stage-12 architecture contract PASS;
- navigation_runtime 18/18 PASS;
- chained transition + physical-limit matrix PASS.

## Latest target-machine attempt

Exact tested checkout:

```
8011cc3ed19fc027fba256ee4aecca7c93a4ce0f
```

Results:
- architecture contract PASS;
- runtime 17/19;
- failures:
  - `navigation_runtime_planner` strengthened continuity regression;
  - `navigation_composite_proving_ground`.

## Decisive composite evidence

The new transverse diagnostics answered the main question.

Newtonian persistent hazard:

iteration 0:
- continuity lateral valid = 1;
- same-branch safe candidates = 0;
- selected branch alignment = 0.0.

iteration 1:
- continuity lateral valid = 1;
- same-branch safe candidates = 0;
- selected branch alignment = -0.976922.

Therefore the planner is **not** abandoning a safe accepted branch.

No safe candidate remains on the accepted transverse branch. The proposed adjusted target is genuinely on another branch.

The correct response is not more branch-ranking heuristics and not forcing a no-stop transit. The higher maneuver layer must recover/brake before changing branch.

## Focused regression fixture defect

The strengthened regression still failed because its exact-static blocker did not actually intersect the deterministic primary -Z probe.

The intended 15 degree / 600 m probe endpoint is approximately:

```
(579.555496, 0, -155.291427)
```

The blocker had been placed near an intermediate guessed location.

It is now centered directly on the actual primary probe endpoint, far from the larger-ring -Z endpoint, so the fixture really tests:
- first-ring preferred branch blocked;
- opposite first-ring branch safe;
- larger-ring preferred branch safe.

## New production escalation signal

Commits:

```
4a91a78bea1e159a329ab346586a4d290ea5d420
9949b701bc8ba181a08b96e8077a375aada725be
7a5b4ae0520a10edbd89fd5f1e81f2427f4768be
e407857d7764300193075cbee941be82312338f8
```

`LocalAvoidancePlanner::Result` and `NavigationRuntimePlanner::Result` now publish:

```
branchSwitchRequired / avoidanceBranchSwitchRequired
```

It is true only when:
- explicit accepted continuity exists;
- transverse continuity is meaningful;
- no safe same-branch candidate exists;
- a safe adjusted target exists on another branch.

This is an escalation signal for maneuver ownership. The local planner does not itself mutate execution or perform braking.

## Focused regression correction

Commit:

```
c8e0c7cd6b6a7deb6c7f618c4d86ea80d4c62400
```

The primary -Z blocker now sits on the real 15-degree probe endpoint.

The regression also asserts that when a safe same-branch larger-ring continuation exists:
- branch switch must NOT be requested.

## Final composite recovery candidate

Commit:

```
0ecf1b71b9620022a49ea71c996f5e81c02e5243
```

When production returns `avoidanceBranchSwitchRequired`:
1. do not execute the opposite-side adjusted target directly;
2. fit a physical Brake recovery from the actual live P/V;
3. recovery is translation-only / fixed-attitude;
4. dense proof requires:
   - peak total brake feed-forward <= 1.35 m/s2;
   - planned dynamic clearance >= 1.50 m;
   - planned static clearance >= 1.50 m;
   - no velocity reversal;
5. execute through B9/B10 -> PilotSkill -> real physics using StateCapture;
6. require:
   - zero tracking-envelope violations;
   - actual dynamic clearance > 0.5 m;
   - actual static clearance > 0.5 m;
   - final speed <= 0.60 m/s;
7. retire the old accepted branch continuity;
8. republish current world truth;
9. call production planner again from the recovered state.

New diagnostic:

```
[COMPOSITE-RECOVERY]
```

reports:
- duration;
- stopping distance;
- peak brake FF;
- planned static/dynamic clearances;
- actual dynamic clearance;
- final speed error;
- tracking violations.

## Architecture meaning

This matches the existing doctrine:

```
accepted moving branch unavailable
 -> do not force impossible continuation
 -> recovery/brake
 -> clear obsolete branch commitment
 -> REPLAN from actual recovered state
```

The navigation module still never gives up.

A branch switch may be required; it simply cannot be accepted as an instantaneous no-stop maneuver when current physical state cannot support it.

## Current gate

Expected suite remains **19 tests**.

If focused regression passes:
- cross-ring same-branch preservation is accepted.

If composite branch-switch recovery passes:
- forced branch changes are physically sequenced rather than hidden inside local visibility steering.

If 19/19:
- accept final composite;
- close synthetic maneuver behavior laboratory;
- move primary evaluation into actual NAV STRESS/game.

## Documentation protocol

After every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 documentation;
- recreate `CONTINUE_PROMPT.md` from scratch.
