# Ship simulation detail / LOD proving ground

The runtime intentionally separates four independent concerns:

```text
Actor identity
    player / NPC pilot / traffic autopilot / drone / none

Lifecycle tier
    TrafficShell / MaterializedShip / Derelict / WreckShell

Motion fidelity
    RouteOnly / Kinematic / Response / FullControl

Internal fidelity
    Response / Detailed
```

The crucial rule is that **FullControl motion does not imply Detailed
internals**. A combat target can fly with authoritative full control while
still using aggregate damage/system state. Detailed module/reactor/repair state
is reserved for gameplay that can actually expose those internals.

## Motion budget

Default observer policy:

- background/unseen: `RouteOnly`
- visible up to ~15 km: `Kinematic`
- clearly observed up to ~4 km or within ~1.2 km regardless of view:
  `Response`
- near the aim axis up to ~3 km or within ~350 m regardless of view:
  `FullControl`
- live combat/projectile/docking/script claims: `FullControl`

Promotion is immediate. Demotion is hysteretic and staged:
`FullControl -> Response -> Kinematic -> RouteOnly`.

The server evaluates the policy at 10 Hz, not at the physics tick rate.

## Server/client authority

Shared gameplay truth remains server-owned. This is required because several
players may observe or shoot one ship simultaneously.

For every shared ship, the server evaluates all player-controlled observer
ships and uses the strongest required MotionFidelity. Therefore one player's
aim/combat demand upgrades the same authoritative entity for every client.

Safe work is moved to clients:

- RouteOnly/Kinematic server truth updates at low cadence;
- client render transforms dead-reckon/interpolate between snapshots;
- proxy rendering/culling stays client/GPU-side;
- maps consume the replicated authoritative ship identity/fidelity.

Clients do **not** decide hit results, collision truth, damage, shared route
state, or the authoritative position of a target.

## Simplified response physics

Materialized Response NPCs use the normal ShipState/control boundary but avoid
re-solving rich structure every tick:

- module -> actuator projection is revision-driven;
- descriptor + actuator + mass/CoM are collapsed into a cached capability /
  effective-physics profile;
- the profile is rebuilt only when construction, actuator damage or mass
  properties change;
- Response NPCs skip detailed reactor/thermal/life-support service simulation
  and per-observer full signal/radar receiver work;
- Response replication omits module/link/hit-volume/sensor payload. Static
  descriptor assembly is enough to render the ship. Detailed promotion sends
  the exact graph; demotion explicitly clears it on the client.

A full NPC that was authored/spawned with persistent mission state is not
automatically collapsed into TrafficShell until that mission state has a
compact shell representation. Traffic-origin entities are marked
`shellEligible` and may freely materialize/dematerialize without losing their
coarse route identity.

Motion cadence is also reduced:

- FullControl: normal Active cadence;
- Response: Prewarm cadence (currently ~10 Hz AI, ~25 Hz control);
- RouteOnly/Kinematic normally live as shells and do not own ShipCore physics.

## Exact tests

Every script mode prints:

```text
[ship-tier-test] source_sha=<40-char git SHA> dirty=<0|1> mode=<mode>
```

and every runtime mode builds `EliteGame` before launching it.

### contracts

```bash
scripts/run-ship-simulation-tier-tests.sh contracts
```

Answers only deterministic questions:

1. lifecycle identity survives shell/materialize/derelict/wreck transitions;
2. CoM/mass state and impulse contracts remain valid;
3. observer policy selects RouteOnly/Kinematic/Response/FullControl correctly;
4. the strongest of several players wins;
5. a live interaction claim forces server FullControl;
6. promotion/demotion hysteresis is correct;
7. MotionFidelity survives replication into System/Detail map sampling;
8. response physics/control contracts still pass.

No performance conclusion is drawn from this test.

### shell500 — pure baseline

```bash
scripts/run-ship-simulation-tier-tests.sh shell500
```

Automatic observer LOD is disabled. Exactly 500 background TrafficShells are
the workload. This measures the cost of identity + low-cadence route motion +
replication + client proxy presentation.

Expected census is approximately:

```text
entities=500 shells=500 materialized=0
motion={route:500,kin:0,response:0,full:0}
```

This is the lower-bound population cost.

### budget500 — real population test

```bash
scripts/run-ship-simulation-tier-tests.sh budget500
```

Starts the same 500 ships as shells, then enables observer-driven detail.
Ships in/near the player's observation/interaction envelope are promoted
automatically; background ships stay cheap.

This is the primary acceptance test. We check:

- the game remains responsive;
- ships are visible and map-visible;
- the population does **not** collapse into 500 FullControl ships;
- `herd_500.csv` shows a plausible mix of four motion levels;
- mean/max slot error remains bounded across LOD transitions.

### tier — lifecycle only

```bash
scripts/run-ship-simulation-tier-tests.sh tier
```

Automatic LOD is disabled. One known entity performs:

```text
TrafficShell
 -> MaterializedShip(Response)
 -> Derelict
 -> WreckShell
```

The EntityId must remain unchanged. This test says nothing about 500-ship
performance.

### mixed500 — compatibility comparison

```bash
scripts/run-ship-simulation-tier-tests.sh mixed500
```

Starts 24 materialized ships + 476 shells, then lets automatic LOD rebalance.
Useful only to compare with earlier logs; it is no longer the primary test.

### fullstress — artificial ceiling

```bash
scripts/run-ship-simulation-tier-tests.sh fullstress
```

Forces all 500 herd members to FullControl/materialized operation. This is a
torture test, not a target architecture. A poor result here does not invalidate
`budget500`.

## herd_500.csv

Columns include:

- total entities;
- shell/materialized counts;
- valid ShipState count;
- RouteOnly/Kinematic/Response/FullControl counts;
- Active/Prewarm/Coarse execution counts;
- mean/max formation-slot error;
- mean speed.

## GPU policy

GPU is useful when the workload stays coherent and massively parallel:

- instanced whole-ship proxy rendering;
- culling;
- future tens-of-thousands shell transform evaluation;
- potentially spatial broad-phase when its results can remain on-GPU.

Do not move authoritative detailed ship simulation to GPU merely because it is
expensive. AI, module damage graphs, repairs, activation transitions and shared
combat authority are branch-heavy and require CPU-side game state. The first
performance wins should remain: **do less work, run it less often, cache it,
and move only presentation to the client**.
