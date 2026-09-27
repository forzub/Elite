# Elite — Autonomous Navigation and Docking Research

Date: 2026-09-27
Status: research / architecture input

## Executive conclusion

External references support keeping the Elite Planner/Follower split. The missing layer is explicit navigation phases plus an independent safety/recovery decision layer.

Recommended structure:

~~~text
StrategicRoute
  -> ManeuverPlanner
  -> AcceptedManeuverProgram
  -> Follower
  -> VehicleController

Parallel:
SafetySupervisor
~~~

For docking:

~~~text
FreeTransit
  -> ProximityOperations
  -> ApproachGate
  -> Hold
  -> DockingCorridor
  -> TerminalCapture
  -> Docked
~~~

Follower tracks an accepted feasible program. It should not invent a new route after a tracking failure. SafetySupervisor should classify the physical situation and select Continue, Hold, Retreat, Escape, Replan or Abort.

## 1. Pioneer

Upstream:
https://github.com/pioneerspacesim/pioneer

Research commit:
c62b938356e37c35d67406b32ebb69d57cf4eeb9

Important files:

~~~text
src/ShipAICmd.cpp
src/ShipAICmd.h
src/ship/Propulsion.cpp
src/ship/Propulsion.h
~~~

Pioneer has high-level AI commands such as FlyTo, FlyAround, Dock, Formation and HoldPosition, with lower propulsion functions for facing, matching velocity, changing velocity and matching angular velocity.

### 1.1 FlyTo derives speed from braking capability

FlyTo continuously considers target position, target velocity, gravity, available acceleration, transverse velocity and moving-target acceleration.

The permitted approach speed is based on remaining distance and available braking, approximately:

~~~text
allowed_speed ~= sqrt(end_speed^2 + 2 * braking_accel * distance)
~~~

Lesson for Elite: the speed profile must be derived from actual ship authority and environment.

### 1.2 Pioneer flips the ship for main-engine braking

During deceleration Pioneer can reverse the desired hull heading so strong forward/main thrust is used for braking.

This directly supports our Newtonian rule:

~~~text
if reverse/lateral authority is insufficient
  rotate hull
  acquire attitude
  use main engine
  brake early enough
~~~

### 1.3 Low-level control respects anisotropic thrust

Pioneer propulsion code limits requested delta-v according to actual directional thrust and external forces. It does not assume arbitrary acceleration in every direction.

Lesson: Follower/controller must operate against the real directional propulsion envelope.

## 2. Pioneer obstacle avoidance

Pioneer does not use one universal 3D pathfinder for every obstacle.

For a large celestial body it checks whether the direct path intersects an unsafe radius. If it does, FlyAround can generate a tangent / orbit-like maneuver before returning toward the target.

Concept:

~~~text
FlyTo target
  -> direct path safe?
      yes -> continue
      no  -> tangent / FlyAround
             -> pass obstacle
             -> resume target
~~~

Useful Elite decomposition:

~~~text
planet or star
  -> specialized tangent / orbital transfer

station
  -> semantic approach corridor

arbitrary local geometry
  -> generic local planner
~~~

One planner does not need to solve every geometry class.

## 3. Pioneer safety recovery

Pioneer has a CheckSuicide-style safety test. It checks whether the current velocity ray intersects an unsafe region and whether available acceleration and gravity still allow stopping.

Important principle: if stopping is no longer the best survival maneuver, recovery may change the velocity direction above the unsafe horizon instead of insisting on stopping.

This agrees with our navigation rule:

- evade if physically possible;
- otherwise brake / redirect as much as possible;
- keep navigation active through recovery.

## 4. Pioneer docking

Docking is staged:

~~~text
Get Approach 1 geometry
 -> FlyTo Approach 1
 -> Get Approach 2 geometry
 -> Final approach
 -> Docking complete
~~~

If the station is far away, ordinary FlyTo is used first.

Then the system obtains clearance and port assignment, reads approach waypoints from station geometry, approaches them, matches relative motion and aligns orientation.

### 4.1 Rotating station

Final approach uses station-relative velocity and station angular velocity. Station orientation is projected forward for the next step.

Lesson for Elite:

The dock is a moving target frame, not a frozen world-space point.

Keep separate:

~~~text
high-level docking intent revision
accepted maneuver-program revision
dock target-frame pose revision
~~~

## 5. Pioneer Autopilot v2 warning

Open draft PR:
https://github.com/pioneerspacesim/pioneer/pull/6345

Research head:
23e20bf7c3eece93d17547f890ae379dc65a92eb

The PR explicitly targets problems where the old generic autopilot behaves badly:

- gravity exceeds available upward thrust;
- matching target velocity around a massive body can cause a fall;
- impact recovery should sometimes fly around instead of stop;
- entering and maintaining orbit deserves a separate maneuver mode.

The experimental branch distinguishes whether the ship is near a planet, whether the destination is visible around the horizon, and whether it should climb into or descend from orbit.

Lesson: even Pioneer is moving away from one universal FlyTo behavior toward semantic maneuver phases.

## 6. Space Engineers historical autopilot

Archived source:
https://github.com/KeenSoftwareHouse/SpaceEngineers

Research commit:
54f2f0f3169cda687a25a438097902a43bdfa603

Important file:

~~~text
Sources/Sandbox.Game/Game/Entities/Blocks/MyRemoteControl.cs
~~~

The route layer is mostly a supplied waypoint sequence. The local loop is roughly:

~~~text
CurrentWaypoint
 -> stopping-distance check
 -> desired direction
 -> collision avoidance
 -> gyro control
 -> thrust control
 -> stuck detection
 -> next waypoint
~~~

This is a useful local controller/safety reference, but not a strong global route planner.

### 6.1 Dynamic stopping horizon

Collision avoidance estimates deceleration from available thrust and ship mass, then derives a stopping distance from speed and deceleration. Look-ahead volumes are built along current velocity.

Lesson:

~~~text
safety horizon =
  function(speed,
           mass,
           braking thrust,
           orientation,
           external acceleration,
           vehicle size)
~~~

Do not use a universal fixed recovery distance.

### 6.2 Terrain safety

The autopilot samples terrain below and ahead of the vehicle and reduces maximum speed as ground clearance decreases.

Useful principle:

~~~text
safe speed = function(clearance, stopping capability, vehicle size)
~~~

### 6.3 Docking mode limitation

The historical docking mode mainly tightens waypoint and orientation tolerances. It is not a complete docking planner.

Lesson: precise arrival at a GPS point is not docking.

## 7. X4 semantic docking geometry

Official modding documentation:
https://wiki.egosoft.com/X4%20Foundations%20Wiki/Modding%20Support/Assets%20Modding/Guides/Dockingbay/

X4 exposes semantic connections:

~~~text
con_todock
  first approach location; normal flight behavior brings the ship here

con_dockpos
  final docking position and orientation

con_launchpos
  undocking exit before normal flight resumes
~~~

Concept:

~~~text
NormalNavigation
 -> ToDock
 -> DockPos
 -> Capture
~~~

Strong lesson: station assets should own semantic docking geometry. Planner should not infer an entrance solely from collision meshes.

For Elite the port can expose:

~~~text
port frame
approach gate
hold point
capture pose
corridor geometry
retreat point
escape direction
terminal speed limits
attitude/rate limits
~~~

The actual trajectory remains calculated from the specific ship capability.

## 8. Naev and transport infrastructure

Upstream:
https://github.com/naev/naev

Research commit:
3163e7b9e50cac28778e2cbb679c3fad84dcd663

Important files:

~~~text
dat/autonav.lua
dat/ai/core/misc/lanes.lua
src/player_autonav.c
~~~

Naev is 2D, so its low-level flight physics are not our model. Its route architecture is useful.

Safe lanes are converted into a graph and a Dijkstra-like route is computed. Travel off lanes is still allowed but gets a larger cost. If lane travel is too large a detour, direct travel wins.

This maps well to future Elite infrastructure:

~~~text
global system/gate graph
system traffic-lane graph
station approach graph
local continuous trajectory planner
~~~

Route cost can include:

~~~text
travel time
fuel / delta-v
risk
congestion
regulation
lane preference
ship class
~~~

Naev also separates approach, brake, landing, follow and boarding phases. Objective and execution phase are different concepts.

## 9. Real autonomous rendezvous and docking

NASA and ESA RPOD systems provide the strongest conceptual reference.

Typical decomposition:

~~~text
Rendezvous
 -> Proximity Operations
 -> Final Approach
 -> Capture / Docking
~~~

Far away, the target can be treated mainly as an orbital/rendezvous target.

Near the target, guidance becomes relative-pose control constrained by keep-out zones, approach corridors, closing rates, attitude and contact conditions.

This is the correct boundary between generic navigation and terminal docking guidance.

## 10. Hold points

ESA ATV used multiple station-keeping points before final docking. HOLD is a normal safe navigation state, not an error.

Recommended Elite sequence:

~~~text
FreeTransit
 -> ApproachGate
 -> HOLD
 -> validate target pose and vehicle state
 -> accept terminal program
 -> terminal ingress
~~~

This gives a deterministic restart point and clean handoff between free navigation and terminal docking.

It also matches our current rule that docking request transfers control to autopilot and stabilizes the ship before final calculation.

## 11. Recovery actions

Real ATV operations distinguish:

~~~text
HOLD
RETREAT
ESCAPE
ABORT
~~~

They are different maneuvers.

Recommended Elite semantics:

~~~text
Continue
  nominal tracking

Hold
  stop relative progress inside a known safe volume

Retreat
  go back along a validated safe corridor / previous hold point

Escape
  prioritize collision separation over nominal route

Replan
  geometry/program is obsolete but immediate danger is absent

Abort
  safe continuation is impossible
~~~

Tracking error itself is not one of these strategies.

## 12. Corridor meaning

A terminal docking corridor should be a constraint on relative state, not only a rendered tube around a centerline.

Monitor at least:

~~~text
cross-track position
along-track progress
closing speed
lateral velocity
attitude error
angular-rate error
stopping-distance margin
keep-out margin
target-frame revision
propulsion feasibility
~~~

The visible tunnel can be a visualization of this admissible state space.

## 13. SafetySupervisor

Recommended architecture:

~~~text
AcceptedProgram
      |
      v
   Follower
      |
      v
VehicleControl

World + vehicle + dock state
      |
      v
SafetySupervisor
      |
      +-- Continue
      +-- Hold
      +-- Retreat
      +-- Escape
      +-- Replan
      +-- Abort
~~~

Follower answers: how do I execute this accepted program?

SafetySupervisor answers: is continuing this program safe and meaningful?

## 14. Phase-specific collision policy

FreeTransit:

- local avoidance allowed;
- tangent detours allowed;
- gradual return allowed;
- replan allowed.

ProximityOperations:

- avoidance must remain in an approved safe region.

TerminalCapture:

- do not improvise a lateral detour around a new obstacle;
- choose Hold, Retreat or Escape;
- replan after reaching a safe state.

## 15. Newtonian and Assisted

External references reinforce that the two modes need distinct feasibility/controller policies.

Newtonian:

~~~text
predict time to rotate
predict attitude acquisition
predict main-engine braking
start flip early
use main engine
~~~

Assisted:

~~~text
desired velocity/course
 -> hull turns toward course
 -> lateral/reverse authority handles transient
 -> main thrust aligns with travel
~~~

They may share high-level route geometry, but not the same maneuver feasibility model.

## 16. Direct implications for current Elite docking

Keep:

- Planner/Follower API boundary;
- AcceptedManeuverProgram;
- target-relative sampling;
- tracking envelopes;
- bounded recovery;
- detailed diagnostics.

Add or formalize:

1. NavigationPhase.
2. DockingPortGuidance with approach, hold, capture, retreat and escape geometry.
3. First-class HOLD before terminal ingress.
4. SafetySupervisor parallel to Follower.
5. Distinct Hold / Retreat / Escape / Replan outcomes.
6. Shared dynamic braking-envelope service.
7. Terminal monitoring in moving dock-port frame.
8. Separate intent, program and target-frame revisions.
9. Phase-specific obstacle policy.
10. Navigation remains active during hold, retreat and replan.
11. Replan does not automatically imply BrakeToStop.
12. Repeated identical failure changes recovery strategy or ends with a diagnostic failure.

Recommended pipeline:

~~~text
DOCK REQUEST
 -> AUTOPILOT OWNERSHIP
 -> STABILIZE
 -> PORT GUIDANCE SNAPSHOT
 -> PLAN APPROACH GATE
 -> EXECUTE + MONITOR
 -> HOLD
 -> VALIDATE MOVING TARGET FRAME
 -> PLAN TERMINAL CORRIDOR
 -> EXECUTE + SAFETY SUPERVISOR
      nominal          -> CAPTURE
      recoverable      -> HOLD / RETREAT -> REPLAN
      imminent danger  -> ESCAPE -> REPLAN
      impossible       -> FAIL
~~~

## 17. What not to copy

Old Pioneer:
Do not put path choice, braking, collision handling, orientation, recovery and termination into one giant FlyTo controller.

Space Engineers:
Do not confuse waypoint following plus reactive avoidance with trajectory planning.

X4:
Do not hard-code the complete trajectory into the asset. The station should provide semantic geometry; Planner should generate a physically feasible trajectory for the actual ship.

## 18. Source lock

Pioneer:
c62b938356e37c35d67406b32ebb69d57cf4eeb9
https://github.com/pioneerspacesim/pioneer

Pioneer Autopilot v2 draft:
23e20bf7c3eece93d17547f890ae379dc65a92eb
https://github.com/pioneerspacesim/pioneer/pull/6345

Naev:
3163e7b9e50cac28778e2cbb679c3fad84dcd663
https://github.com/naev/naev

Space Engineers historical source:
54f2f0f3169cda687a25a438097902a43bdfa603
https://github.com/KeenSoftwareHouse/SpaceEngineers

X4 docking geometry:
https://wiki.egosoft.com/X4%20Foundations%20Wiki/Modding%20Support/Assets%20Modding/Guides/Dockingbay/

NASA RPOD:
https://www.nasa.gov/reference/jsc-rendezvous-prox-ops-docking-subsystems/

ESA ATV:
https://www.esa.int/content/view/full/153253

## Bottom line

The strongest lesson for the current Elite docking failure mode is:

Tracking error is a diagnostic condition, not a recovery strategy.

First classify the safety state. Then choose Hold, Retreat, Escape or Replan. Only that selected recovery action should produce the next accepted maneuver program.

The Planner/Follower split is worth preserving. The next structural improvement is explicit navigation phases, port-owned semantic guidance geometry and an independent SafetySupervisor.
