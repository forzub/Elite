# Elite — CURRENT TASK

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** 10 — deterministic `PilotSkillProfile` execution gate

## Progress

```text
[██████████████████░░░░░] 9 / 12 major stages closed

1–9  CLOSED / ACCEPTED
10   ACTIVE — PilotSkillProfile execution
11   PENDING — live integration + physics hookup
12   PENDING — stress/debug + legacy retirement
```

## Newly closed — moving/rotating docking stage 9

Final target-machine evidence:

```text
c90a66d6c64bdf3acc037208a000b1955d40e6c3
NAVIGATION TRAJECTORY DOCKING APPROACH CONTRACT: PASS
10/10 navigation_trajectory CTest PASS
100% tests passed
```

Decision: `DockingApproachEvaluator` is behavior/architecture accepted. Stage 9A + 9B are frozen unless live integration exposes a defect.

## Active candidate — `PilotSkillExecutor`

Code:

```text
src/world/navigation/control/PilotSkillExecutor.h
src/world/navigation/control/PilotSkillExecutor.cpp
src/world/navigation/control/CMakeLists.txt
```

Contract:

```text
src/world/navigation/PILOT_SKILL_MODEL.md
```

Tests:

```text
tests/navigation_trajectory/NavigationPilotSkillTests.cpp
tests/architecture_contracts/check_navigation_pilot_skill.py
```

### Required separation

```text
world truth          != pilot skill
vehicle capability   != pilot skill
navigation intent    != pilot skill
pilot execution      = delay/cadence/latency/damping/precision
physics truth        remains downstream authority
```

No skill scalar is allowed to shrink obstacles, enlarge gaps, change capture tolerance or grant thrust.

### Command timing

A new maneuver revision:

```text
observed
 -> reaction delay
 -> perception/decision tick
 -> fixed command-latency queue
 -> active target
 -> second-order execution response
```

Commands evolving inside the same maneuver revision are sample-and-hold at `perceptionDecisionRateHz`.

High-urgency emergency commands may shorten reaction delay using:

```text
emergencyResponseThreshold01
emergencyReactionDelayScale
```

### Dynamics

Command response is deterministic second order:

```text
y'' = wn^2(target-y) - 2*zeta*wn*y'
wn = 2*pi*responseFrequencyHz
```

with explicit gain and linear/angular command slew limits.

Low damping can therefore produce real command overshoot/ringing. The ship only moves when downstream physics integrates the resulting demand.

### Deterministic precision error

Noise source:

```text
seed + intent revision + decision sequence + axis
```

No wall clock or `std::random`.

### Closed-loop fixture

A small 1D docking-like plant receives the same ideal PD intent.

Expert:

```text
0 reaction
60 Hz decisions
0 latency
4 Hz response
damping 1.0
gain 1.0
```

must settle close to the target.

Poor:

```text
0.20 s reaction
8 Hz decisions
0.15 s latency
1 Hz response
damping 0.20
gain 1.40
limited slew
```

must repeatedly cross the target and remain less settled by the same deadline.

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_pilot_skill.py
bash tests/navigation_trajectory/run_mingw64.sh
```

Expected suite count:

```text
11/11
```

## Next after green

1. accept/freeze stage 10;
2. progress becomes 10/12;
3. begin live `EliteGame` / `EliteServer` / guidance + flight/physics integration;
4. end-to-end stress/debug/performance;
5. retire legacy navigation after stable v2 ownership.
