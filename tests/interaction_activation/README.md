# Server interaction activation

This suite protects the server-side interaction/activation policy.

## Stage 3A — interaction math

The policy uses existing `LogicalDimensions` to build conservative broad
bounds and predicts closest approach over a finite interaction horizon. Radar,
sensors, transponders, communications and client presentation are deliberately
outside this subsystem.

## Stage 3B — physical shadow demand

`GameSimulation` evaluates, without enforcing, the physical mode each real ship
would request:

- player -> `Active` (pinned),
- currently inside an interaction envelope -> `Active`,
- predicted to enter an envelope within the look-ahead window -> `Prewarm`,
- otherwise -> `Coarse`.

Interaction anchors currently include ships and static infrastructure in the
same active system. Static objects use their existing logical dimensions, so a
multi-kilometre station naturally becomes relevant earlier than a small ship.
Whole-station simulation activation is intentionally not implemented here;
large infrastructure will later activate by spatial sectors.

## Stage 3C — stabilized activation plan

The raw physical request is now passed through two additional layers before it
can ever become a production scheduling decision:

1. **Gameplay claims** may only raise demand to `Prewarm` or `Active`. Combat,
   projectile threats, docking and explicitly critical scripted interactions
   are represented here. Radar/sensor visibility is not a claim.
2. **Demotion hysteresis** makes promotion immediate but release gradual:
   `Active -> Prewarm -> Coarse`. This prevents boundary chatter and gives
   tactical runtime state time to settle.

## Stage 3D — spatial candidate broad-phase

The planner now rebuilds a conservative system-local spatial hash and queries
only anchors that can possibly enter the subject's interaction envelope within
the configured look-ahead window. The query radius includes subject size, the
maximum anchor size in the system, gameplay/safety reach and a conservative
relative travel distance. The velocity bound is evaluated around a per-system
co-moving velocity origin so shared orbital bulk motion does not inflate the
query. Exact CPA evaluation still decides `Active`,
`Prewarm` or `Coarse`; the spatial index only removes impossible candidates.

Pathological query spans fall back to all anchors in the same system rather
than sacrificing correctness. Candidate order is restored to anchor insertion
order so exact ties remain deterministic.

Stage 3D is still planning-only. `current_mode` in the diagnostic CSV therefore
remains truthfully `Active`: no production AI, physics, signal, sensor or
snapshot loop may consume `planned_mode` yet. The planner stays at 5 Hz until
the first production gating stage is validated.

While `ActivationShadowDiagnosticsEnabled` is true, the server writes:

`simulation_activation_shadow.csv`

The Stage 3D capture includes physical demand, gameplay-raised demand,
stabilized planned mode, durable transition serial/time, broad-phase candidate
counts, query radius/cell count, co-moving residual speeds and fallback state.

Run:

```bash
bash tests/interaction_activation/run_mingw64.sh
```

Validate a real capture with:

```bash
python tests/interaction_activation/verify_shadow_capture.py simulation_activation_shadow.csv
```
