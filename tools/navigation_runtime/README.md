# Navigation Runtime — live 3D diagnostic stand

Location: `tools/navigation_runtime/`.

This tool is no longer a passive trace viewer. The JSON file describes only the
input scenario. The executable calculates the route and vehicle motion itself by
running the production navigation/control/physics chain in-process.

## Runtime chain

On `РАССЧИТАТЬ` the tool runs:

```text
scenario.json
  -> NavigationSpace / NavigationMap
  -> NavigationRuntimePlanner
  -> AcceptedManeuverProgram
  -> TrajectoryFollower
  -> NavigationRuntimeControlBridge / PilotSkillExecutor
  -> SharedShipPhysics / DynamicMotionSystem
  -> calculated TraceDocument in memory
  -> 3D playback
```

The calculated trace is also saved as
`tools/navigation_runtime/last_calculated_trace.json` for diagnostics, but it is
an output, not an input to the live stand.

## Top controls

### Режим управления
- `АССИСТЕД`
- `НЬЮТОН`

This changes the real local flight control law used by physics and the maneuver
attitude policy.

### Пилот
- `ЭКСПЕРТ`
- `СРЕДНИЙ`
- `ЛУЗЕР`

This changes the real PilotSkillExecutor profile: reaction delay, decision rate,
command latency, response bandwidth, slew and deterministic command error.

### Режим полёта
- `СТАНДАРТ`
- `ЭКСТРИМ`

This changes cruise speed and planner cost/horizon/aggressiveness parameters.
Hard collision geometry and vehicle authority are not disabled.

### Внезапная помеха
Unchecked: the sudden obstacle is never published to NavigationMap.

Checked: the sudden obstacle is absent from the initial world and is published
only when its `activation_time_s` is reached during simulation. Therefore the
initial route is calculated without foreknowledge of the surprise obstacle.
After appearance, normal receding-horizon replanning sees it and reacts.

### РАССЧИТАТЬ
Runs the full scenario from the start state using the currently selected modes.
The resulting route and motion replace the previous calculation.

## scenario.json

Default file:
`tools/navigation_runtime/scenario.json`

Supported inputs:

- start position, velocity, forward and up;
- optional forced ship route points (normally empty so Planner chooses the route);
- final position;
- optional required final forward direction;
- optional required final up direction;
- required final speed;
- standard/extreme cruise speed;
- static obstacles:
  - sphere;
  - box;
  - capsule;
- moving obstacles:
  - initial position + velocity vector; or
  - route points + route speed;
- one optional sudden obstacle with the same motion description;
- sudden obstacle may spawn relative to the live Cobra body using
  `spawn_relative_to_ship_fru = [forward, right, up]`.

## 3D diagnostics

The stand renders:

- static obstacles from JSON;
- route selected by Planner as a white polyline;
- local adjusted/bypass targets;
- actual Cobra path;
- actual Cobra body basis and nose;
- AcceptedManeuverProgram reference nose;
- angular error between actual and program reference;
- translucent AcceptedManeuverProgram tracking corridor;
- moving obstacle and predicted envelope;
- replan events;
- bottom-left `ГОРИЗОНТ КОБРЫ` projection plane;
- frame slider and playback controls.

The tracking corridor is deliberately labeled as the maneuver-program follower
envelope. NavigationRuntimePlanner::Result does not currently publish a separate
volumetric planner corridor, so the tool does not invent one.

## Cobra horizon inset

The inset is the plane perpendicular to the Cobra's current longitudinal axis.

The moving obstacle is projected into Cobra right/up coordinates. Repeated
projected envelopes over the planner look-ahead form the visible predicted
obstacle tunnel.

## Window behavior

The viewer opens as an ordinary decorated Windows window maximized to the desktop
work area. It is not exclusive fullscreen and not borderless game fullscreen.

## Build

From MSYS2 MinGW64:

```bash
cd /d/__elite/work
cmake -S tools/navigation_runtime -B build/tools/navigation_runtime -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tools/navigation_runtime
```

## Run executable

Default scenario:

```bash
cd /d/__elite/work
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Custom scenario:

```bash
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe path/to/my_scenario.json
```

Or build and run in one command:

```bash
bash tools/navigation_runtime/run_mingw64.sh
```

The helper prints the exact executable launch command before starting it.
