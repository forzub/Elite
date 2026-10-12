# Ship physical control model contract

Status: foundation contract, 2026-10-12.

## Purpose

This layer exists to make ordinary local-flight behavior a consequence of the
ship's installed propulsion topology instead of a freely switchable control-law
constant.

It is intentionally a functional physical model, not an engineering model.
Ship descriptors author performance ratings; they do not author arbitrary
engine geometry.

## Fixed model set

Exactly six ordinary-flight physical models exist:

| Model | Main propulsion slots | Manoeuvre slots |
| --- | ---: | ---: |
| Assisted-I | 2 aft + 2 fore vectoring main | 0 |
| Assisted-Y | 3 aft + 3 fore vectoring main | 0 |
| Assisted-X | 4 aft + 4 fore vectoring main | 0 |
| Newtonian-I | 1 aft fixed-axis main | 4 functional RCS clusters |
| Newtonian-Y | 3 aft fixed-axis main | 6 functional RCS clusters |
| Newtonian-X | 4 aft fixed-axis main | 8 functional RCS clusters |

A descriptor selects one `PhysicalControlModelId`. It may tune nominal
acceleration, rate/load envelopes and later component ratings. It may not add,
remove or reposition propulsion slots.

A descriptor may bind those fixed slots to damageable module IDs through
`physicalControl.slotModuleIds`. Binding order is the topology's slot order;
this maps damage state onto the fixed model and does not create geometry.

The I/Y/X coordinates are normalized functional lever arms. They are not CAD
dimensions and must never require per-ship engineering calculations.

Canonical body basis is the existing runtime basis:

```text
+X = right
+Y = up
-Z = forward
+Z = aft
```

## Assisted family

Assisted has matched fore and aft vectoring main-engine banks.

The flight-control computer owns velocity control:

- aft main bank accelerates toward forward velocity;
- fore main bank removes forward velocity / accelerates in reverse;
- vectoring of the same main engines supplies lateral translation and attitude
  authority;
- no dedicated ordinary RCS slots exist in the Assisted physical models.

A healthy Assisted craft therefore has hardware-level bidirectional
longitudinal authority and full local 6-DOF control.

Loss of slots changes capability but never changes construction family:

```text
FullAssisted
    -> DegradedAssisted
    -> ThrustAxisFallback
    -> Uncontrollable
```

For example, loss of the entire fore bank means the ship remains an Assisted
construction but must use thrust-axis / flip-and-burn doctrine for strong
braking.

## Newtonian family

Newtonian has one dominant aft fixed-axis propulsion bank and no fore main
bank.

- I uses one central aft main engine;
- Y uses three symmetric aft main engines;
- X uses four symmetric aft main engines;
- low-authority manoeuvre clusters provide attitude and small translational
  corrections.

The main bank supplies high axial acceleration. Large velocity changes that
oppose the current thrust axis require hull reorientation and burn.

Runtime degradation is:

```text
FullNewtonian
    -> DegradedNewtonian
    -> SurvivalControl
    -> Uncontrollable
```

## Current simplified physics

The first implementation deliberately works in kinematic authority rather than
engineering detail.

Each topology owns a fixed set of semantic actuator slots and normalized lever
positions. Slot state is currently binary: operational or unavailable.

The model derives:

- forward/reverse main authority;
- lateral/vertical authority estimate;
- pitch/yaw/roll authority estimate;
- runtime flight regime;
- a temporary `ShipParams` compatibility projection for the existing motion
  stack.

Healthy aggregate ratings come from the ship descriptor. Losing slots reduces
the corresponding authority. This is intentionally an inverse-design model:
the descriptor states what a healthy ship can do; the fixed topology defines
how failures remove portions of that capability.

The compatibility projection is temporary. It exists so the physical models
can be introduced behind `ShipControlExecutionGateway` without rewriting
networking, autopilot or prediction at the same time.

## Explicitly out of scope

This layer does NOT calculate:

- reactor load or electrical power;
- fuel consumption;
- manoeuvre reaction mass / working fluid;
- thermal generation or cooling;
- detailed nozzle mechanics;
- structural stress;
- CAD dimensions or material properties.

Those may constrain the same actuator slots later, but they are separate plant
layers.

## Jump drive boundary

Jump/cruise propulsion is NOT ordinary Newtonian local-flight propulsion.

No jump-drive type, state, power rule, speed rule or control command belongs in
the Assisted/Newtonian physical control models. Jump propulsion will be
designed as a separate mechanic and may only hand off to local flight at an
explicit boundary.

## Descriptor ownership

`ShipDescriptor::physicalControlModel` is the static construction truth.

`ShipDescriptor::localFlightControlLaw` is currently only a compatibility
mirror and must equal `nativeFlightLaw(physicalControlModel)` at ordinary
initialization.

Runtime damage may change `RuntimeFlightRegime` and the capability snapshot.
It must not rewrite `physicalControlModel`.

## Migration rule

The accepted ship-control gateway and corridor stress remain the regression
boundary.

The new physical modules are introduced behind
`ShipControlExecutionGateway`. Human input, autopilot output, network
`ShipControlState`, server authority and client reconciliation are not
redesigned as part of this layer.
