# Elite — CURRENT STATE

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** 12A-3 — authoritative live obstacle behavior proof

## Progress

```text
[██████████████████████░] 11 / 12 major stages closed

1–11 ACCEPTED
12 ACTIVE
   12A-1 live NavigationWorld composition seam       ACCEPTED
   12A-2 authoritative GameSimulation proving actor  ACCEPTED
   12A-3 live CUBE 08 behavior proof                 CANDIDATE
```

## 12A-2 acceptance

Target-machine evidence on:

```text
7b4db95788d80c95afb3b57c109c671cb7a41366
```

passed:

```text
NAVIGATION STAGE 12 RUNTIME PLANNER CONTRACT: PASS
navigation_runtime 3/3 PASS
EliteGame build PASS
EliteServer build PASS
```

This accepts the authoritative `GameSimulation` wiring, isolated Active lab
actor, real NAV STRESS hit-volume source, and shared client/server production
build.

## 12A-3 candidate

Code baseline:

```text
27282d1d0d5e38072906139afbd743f712a1a68a
```

A dedicated real-server mode now exists:

```text
EliteServer --self-test-navigation
```

It runs the actual `ServerRuntime`, fixed-step authoritative physics,
replication and the existing `NAVIGATION V2 RUNTIME LAB` actor.

The evidence chain is:

```text
NAV STRESS CUBE 08
 -> HitVolume-derived NavigationMap candidate
 -> nominal conflict identity retained
 -> adjusted local target
 -> NavigationRuntimeControlBridge
 -> PilotSkillExecutor executed lateral acceleration
 -> authoritative ship motion
 -> positive conservative obstacle clearance
 -> continued goal progress
 -> replicated execution vector == authoritative execution vector
```

The local dynamic broadphase now uses a bounded `NavigationMap::querySphere()`
covering the complete physical local horizon/probe fan. It no longer samples
only the nominal straight corridor.

Exact HitVolume OBB -> static/precision NavigationSpace topology remains the
next geometry slice after this live-behavior gate.


## 12A-3 first live result

The first target-machine live run on
`d1eb73e5335252be16e56ff2cf54f6cc48f5b2c4` passed architecture,
`navigation_local 2/2`, `navigation_runtime 3/3`, EliteGame and EliteServer.

The self-test then failed only at its replication comparison:

```text
[FAIL] navigation-runtime replicated execution differs from authoritative truth
error_mps2=0.00497292
```

That comparison was invalid: the self-test compared the latest retained client
execution snapshot with the newest per-fixed-step diagnostic observation, which
can legitimately belong to different epochs under sparse publication cadence.

The candidate now waits for a sparse packet that actually publishes the lab
ship, copies GameServer's authoritative published snapshot in the same
ServerRuntime step, requires equal `serverTick`, and compares the execution
product at that exact publication epoch. Canonical sparse hydration is checked
against the same source as a separate invariant.

No tolerance was weakened.
