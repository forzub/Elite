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
51e6c41bb94b65e8cc269fb035164a4eb0aa23fd
```

Results:
- architecture contract PASS;
- `navigation_runtime_planner` PASS, including explicit accepted-segment continuity regression;
- runtime 18/19;
- only `navigation_composite_proving_ground` failed.

## What the latest run proved

The explicit continuity hint is correctly wired through production:
- focused planner regression is green;
- `NavigationRuntimePlanner` receives accepted local continuity;
- `LocalAvoidancePlanner` consumes it.

However the final composite still changed branch.

Logged Newtonian continuation:
```
continuity=(0.357512,-0.349550,+0.866025)
current position=(181.656981,57.946838,25.984828)
new target=(172.399109,59.234718,-2.521893)
```

The new target is almost opposite the +Z continuity branch.

## Root cause

The previous production fix only used continuity **inside each individual deflection ring**.

The algorithm still preserved this outer ordering:

```
15 deg ring
 -> if any safe candidate exists, return
30 deg ring
45 deg ring
...
```

Therefore an opposite-side candidate on a smaller ring could beat a same-branch candidate on a slightly larger ring.

That violates the meaning of an explicit accepted-segment continuity contract.

## Current unverified production correction

Commit:

```
e19c1804806ce5f3554f20c7b7d3d5ac19b4911e
```

New semantics when explicit continuity is present:
- evaluate all safe candidates across all allowed ordinary deflection rings;
- safe same-branch candidates outrank opposite-branch candidates;
- among candidates in the same branch class, higher alignment with accepted direction wins;
- smaller deflection is secondary tie-break;
- deterministic azimuth index remains final tie-break;
- if no safe same-branch candidate exists anywhere, planner may choose the least-opposed safe candidate rather than deadlock.

Without explicit continuity:
- legacy smallest-safe-ring priority remains;
- current velocity only ranks candidates inside that ring.

No safety, clearance, horizon or maximum-deflection bound changed.

Public API contract comment updated in:
```
c3dcf98b16bd6e45f0dbc949ec926f086aa623a0
```

## Stronger focused regression

Commit:

```
06a917f058926a29f41a936dd994be3f8073e7cf
```

The regression now proves cross-ring semantics:
- accepted continuity points toward -Z;
- an exact-static blocker rejects only the preferred -Z candidate on the first 15 deg ring;
- the opposite +Z first-ring candidate remains safe;
- the preferred -Z branch is safe again on a larger ring;
- planner must choose the larger same-branch ring instead of the smaller opposite-side ring.

## Composite diagnostics and ownership correction

Diagnostics commit:
```
e62328d4af99b6e452e2d07e53cd20e25a103dcf
```

`[COMPOSITE-RESUME]` now prints selected deflection degrees.

Ownership commit:
```
a2da6453daaa8e00cc1f4661321b9294513057ff
```

Continuity is now updated only after the corresponding physical program:
- is physically authorable;
- executes successfully;
- preserves required clearance.

The hint is taken from the executed program start->terminal displacement.

Thus continuity is owned by executed accepted state, not merely by a planner proposal.

## Current gate

Expected suite remains **19 tests**.

If focused regression fails:
- debug global cross-ring branch ranking.

If focused regression passes but composite still switches branch:
- direction vector is insufficient and the next contract should be an explicit accepted local branch/plane identity.

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
