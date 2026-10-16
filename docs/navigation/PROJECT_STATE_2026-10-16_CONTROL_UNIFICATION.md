# PROJECT STATE — CONTROL UNIFICATION / NEWTONIAN ENTRY — 2026-10-16

## Status

The project has crossed the Assisted-finalization boundary and is now at the
control-unification gate immediately before real Newtonian V1 planner work.

The previous document
`PROJECT_STATE_2026-10-16_ASSISTED_FINALIZATION.md` describes the temporary
Assisted revalidation phase and is retained as historical context. This document
supersedes it for current work.

## Hard production control architecture

Production automatic flight is a virtual pilot.

```text
human input ---------\
                      -> ShipControlState -> flight-control law
autopilot / NPC -----/                       -> actuator allocation
                                             -> physical plant
                                             -> integration
```

Rules:

- autopilot replaces the human as ordinary `ShipControlState` producer;
- autopilot does not command propulsion hardware directly;
- human, autopilot and NPC controls share identical flight-control semantics;
- physical capability remains below this seam in physical model / actuator state /
  plant;
- legacy direct navigation-acceleration fields are diagnostic/migration seams only,
  not production automatic-docking control.

## Assisted status

Assisted-I/Y/X live game matrix has passed with the actual replicated model and law
matching the requested construction.

Observed healthy capability:

- Assisted-I: 4 propulsion slots;
- Assisted-Y: 6 propulsion slots;
- Assisted-X: 8 propulsion slots;
- all three report Assisted law with forward and reverse main authority.

The live Assisted safe-course / planning / handoff sequence is visually working in
the latest tested build.

Assisted control semantics remain frozen:

- hull/nose is commanded toward the required course first;
- ordinary FA-on bends velocity toward the nose;
- no direct velocity rewrite;
- safe-course selects one course and does not continuously chase a recovery arc;
- expensive planning runs while the ship physically coasts;
- late entry preserves the downstream backbone and rebinds only the entry.

Known performance concern, not currently a correctness failure:

- expensive backbone planning may take ~13 s;
- late-entry rebind has been observed taking ~8 s;
- projected handoff can add additional coast time.

The rebind cost is still suspicious for a supposedly cheap connector and should be
profiled after the current control-law gate is closed.

## Physical model / diagnostic switching

Temporary runtime controls remain:

- Ctrl+F9: I / Y / X;
- Ctrl+F10: Assisted / Newtonian.

The switch changes authoritative construction, actuator topology, native law and
capability. HUD propulsion-slot count changes with the selected layout.

The cockpit terminal is presentation-only. Model switches may append model/topology
information but must not control terminal visibility or reset its scrolling history.

## Newtonian control-law status

Newtonian planner V1 is not yet the active development target.

Before planner work, the basic controllable ship is being completed.

Accepted Newtonian semantics now are:

### Main propulsion

- main thrust released -> translational velocity remains inertial;
- no Assisted-style global velocity stabilization;
- braking still requires physically valid Newtonian manoeuvres such as turn/flip
  plus thrust.

### Attitude controls

- released pitch/yaw/roll -> target angular rate becomes zero;
- rotation is arrested using bounded physical angular authority;
- no direct orientation or angular-velocity rewrite.

### Manoeuvre / precision translation controls

Target product behavior:

- while strafe/lift/precision translation controls are held, real manoeuvre
  thrusters produce bounded delta-v;
- when released, the Newtonian control law physically removes the manoeuvre motion
  created by those controls so that fine human docking remains practical;
- this is flight-control behavior shared by human, autopilot and NPC sources;
- it must not become an Assisted global FA-on controller;
- main-engine / pre-existing inertial velocity must remain Newtonian.

Current implementation introduces a Newtonian manoeuvre delta-v ledger and bounded
RCS counter-thrust. This implementation has a dedicated regression test but has not
yet been accepted as the final architectural form. It must be validated against the
hard pilot-replacement contract before Newtonian planner work proceeds.

## Terminal status

The cockpit terminal now supports richer presentation diagnostics:

- persistent scrolling history;
- ship model / law / topology lines;
- planner stage/progress/worker information;
- deterministic decorative CALC/HEX activity;
- error/warning priority over decorative activity.

Terminal code remains presentation-only and must not alter navigation or physics.

## Immediate gate

Before resuming Newtonian V1 planner development:

1. validate Newtonian manoeuvre self-stabilization in the contract test and live game;
2. confirm it behaves identically when the control sample comes from autopilot;
3. ensure physical capability remains truthful and no hidden engine bypass exists;
4. run the narrow Assisted regressions affected by shared control code;
5. create a new recovery point.

After this gate, Newtonian V1 can resume from the existing planning charter:

- pure manoeuvre solver first;
- explicit coast / rotate / burn / flip / brake sequence;
- forward-simulated proof of terminal state;
- only then obstacle-rich route planning and docking integration.

## Current development position

In one sentence:

**The shared pilot/control/plant architecture and Assisted flight path are largely
established; the project is finishing Newtonian vehicle controllability before
starting the real Newtonian manoeuvre planner.**
