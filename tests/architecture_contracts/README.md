# Cross-timeline and diagnostic architecture contracts

Run from the repository root under MSYS2 MinGW64:

```bash
bash tests/architecture_contracts/run_mingw64.sh
```

This suite closes gaps that were not covered by the earlier clock and map
architecture checks. It locks down:

- normal -> accelerated -> normal `UniverseClock` rewind semantics;
- transactional multi-ship diagnostic state that is discarded rather than
  committed to production state;
- universe-timeline revision as an interpolation/transition fence;
- deterministic absolute-time orbital state after a rewind;
- observable cloud debug-speed behavior, including stop/x5 and km->m units;
- source-level ownership rules preventing debug-only trajectory state from
  leaking back into `DynamicMotionState`;
- pre-input synchronization before map-frame preparation;
- non-persistence of an active diagnostic session in Debug Control defaults.

The existing `world_runtime` and `system_map` suites remain authoritative for
their established contracts. This suite is intentionally cross-cutting: it
checks the seams between them, because those seams are where a monotonic server
clock and a rewinding universe timeline can otherwise produce mixed-branch
frames.

## Hub -> Detail across timeline revision fences

A revision change invalidates branch-local Detail/Hub snapshot bytes but keeps
the semantic `m_loadedDetailTarget`. Returning from Hub to Detail must therefore
reacquire that target on the active revision when the cached Detail snapshot was
invalidated. Navigation semantics may not depend on cache lifetime.

## Stage 10A: first-class system membership

Dynamic and static runtime objects now carry explicit star-system membership.
`WorldPosition` remains system-local, so numeric coordinates may never be used
to infer that two entities share a system. The Stage-10A contract checks that:

- ships, hub/reference frames, diagnostic trajectory states and snapshots carry
  `systemId` end to end;
- static-object spatial membership is separate from System-map visibility;
- player navigation derives its current system from the authoritative player
  ship rather than maintaining an unrelated mutable copy;
- System, Detail and Hub snapshot builders reject entities from other systems;
- System-map snapshots publish real ships without inventing orbit metadata;
- promo/debug helpers cannot mutate the frozen production branch while
  accelerated diagnostics are active.

This is a prerequisite for Migration Stage 3. It does not yet implement
inter-system transfer or per-system dynamic simulation contexts.
