# Project State

**Updated:** 2026-09-18 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / deterministic NPC pilot execution  
**Canonical development branch:** `main`  
**Active stage:** 10 — `PilotSkillProfile`

## Progress

```text
[██████████████████░░░░░] 9 / 12 major stages closed
```

Closed:

```text
1  NavigationMap / mass dynamic state
2  NavigationSpace / global corridors
3  LocalHorizon / LocalAvoidance
4  oriented passages / bounded gaps / attitude reachability
5  continuous static passage
6  emergency mitigation / contact severity
7  moving gap prediction
8  moving continuous oriented-hull passage
9  moving/rotating docking 6DoF
```

Active / remaining:

```text
10 deterministic PilotSkillProfile execution   ACTIVE
11 live game/server/guidance + physics hookup  PENDING
12 end-to-end stress/debug + legacy retirement PENDING
```

## Accepted docking evidence

```text
terminal 9A
835271539619b7dd02efc54ff54df51d64b49fce
9/9 PASS

continuous approach 9B
c90a66d6c64bdf3acc037208a000b1955d40e6c3
NAVIGATION TRAJECTORY DOCKING APPROACH CONTRACT: PASS
10/10 PASS
```

Stage 9 is closed.

## Active stage 10 architecture

`PilotSkillExecutor` sits between accepted navigation/control intent and real flight control:

```text
accepted ideal acceleration intent
        |
        v
PilotSkillExecutor
  reaction delay
  decision cadence
  command latency
  gain / damping / slew
  deterministic command-space precision error
        |
        v
flight-control/thruster authority
        |
        v
physics
```

It is stateful per pilot but fixed-size and backend-neutral.

The game-facing `PilotSkillProfile` separates:

```text
ExecutionProfile
    timing / response / precision

PolicyProfile
    anticipation / risk / comfort
```

The execution layer does not consume policy preferences. This prevents preference knobs from becoming hidden physics modifiers.

## Pinned stage-10 behaviors

```text
reaction/latency delay
sample-and-hold decision cadence
urgent emergency reaction shortening
seeded replay-stable command error
damping-controlled overshoot
poor-pilot closed-loop oscillation
policy/execution ownership separation
```

## Performance/scaling direction

The isolated executor has bounded per-call work:

```text
queue <= 256
integration substeps <= 64
no world queries
no allocations in steady state
```

Mass-NPC scheduling/performance is intentionally validated in stage 11/12 live composition rather than inventing a synthetic world benchmark here.

## Next order

1. target-machine stage-10 architecture + 11/11 behavior gate;
2. live `EliteGame` / `EliteServer` / guidance + flight/physics integration;
3. end-to-end stress/debug/performance;
4. retire legacy route-wide navigation when v2 is live-stable.
