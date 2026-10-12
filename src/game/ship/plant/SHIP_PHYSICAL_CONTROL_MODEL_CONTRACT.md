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
| Newtonian-I | 1 aft vectoring main | 4 functional RCS clusters |
| Newtonian-Y | 3 aft vectoring main | 6 functional RCS clusters |
| Newtonian-X | 4 aft vectoring main | 8 functional RCS clusters |

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

Newtonian has one dominant aft thrust-vectoring propulsion bank and no fore
main bank.

- I uses one central gimbaled aft main engine;
- Y uses three symmetric gimbaled aft main engines;
- X uses four symmetric gimbaled aft main engines;
- low-authority manoeuvre clusters provide attitude and small translational
  corrections independently of main-engine burn.

Main-engine TVC can bend the trajectory while accelerating. In I layout the
single centred main couples lateral thrust strongly to pitch/yaw and provides
no useful direct roll moment. Y/X layouts use multiple off-axis vectoring mains,
so differential/vectored thrust spans a richer powered force/torque space,
including roll authority. This powered steering is separate from RCS authority
because it exists only while the main propulsion bank is producing thrust.

The main bank supplies high axial acceleration. Large velocity changes that
oppose the current thrust axis still require hull reorientation and burn.

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

The snapshot distinguishes independent kinematic authority from
`poweredMainVectoring`. For Newtonian craft, TVC trajectory bending and
main-engine attitude authority are burn-coupled and must not be mistaken for
free high-authority strafe while coasting.

### Assisted thrust utilization

Assisted lateral translation and some attitude manoeuvres may require fore and
aft main engines to fire against each other. The engines themselves are not
made thermodynamically inefficient; rather, part of their generated thrust
cancels in the net force/wrench.

The simplified model therefore publishes a propulsive utilization estimate:

```text
lateralTranslation01 =
    useful net lateral acceleration /
    gross opposed fore+aft acceleration authority

cancelledDuringLateral01 =
    1 - lateralTranslation01
```

This value is intended for later fuel/power/thermal accounting. It must not be
confused with engine efficiency. Axial acceleration has no opposed-bank
cancellation penalty. Newtonian main TVC redirects thrust rather than cancelling
a fore/aft pair; its cost is loss of axial thrust component as the vector is
deflected, which will be modeled when vector angle is introduced.

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
