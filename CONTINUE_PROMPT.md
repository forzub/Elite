# CONTINUE PROMPT — Elite Navigation v2 / finish docking gate, then wire production Automatic docking

Work in public repository `forzub/Elite`, branch `main`.

Read newest sections of `CURRENT_STATE.md`, `CURRENT_TASK.md`,
`PROJECT_STATE.md`, and `src/game/navigation/STAGE12_END_TO_END.md` before
changing behavior. Synchronize them after every state-affecting result and
regenerate this prompt every iteration.

## Immediate gate

Run:
```bash
git pull --ff-only origin main
bash verify_docking.sh
```

Current planner fix under test: shortened preferred final-axis joins retreat
away from obstacle contact boundaries and retry routing progressively toward the
mandatory ingress.

## Automatic docking current truth

Automatic docking is not yet production-wired.

Exists:
- DockingRouteRequest::Mode::Automatic;
- server Autopilot ownership;
- AcceptedManeuverProgram;
- TrajectoryFollower;
- NavigationRuntimeControlBridge;
- navigation acceleration demand in ShipControlState;
- shared physical capability enforcement.

Missing:
- server-owned player accepted-program execution lifetime;
- fixed-step follower/bridge advancement for the player ship;
- Automatic request integration in SpaceState;
- START DOCKING UI enablement;
- completion/replan/stop/handoff lifecycle.

Do NOT implement gate-frame chasing.

## Next implementation after docking verifier passes

1. retain server Autopilot ownership after preparation for Automatic requests;
2. create/store accepted proved maneuver program state per controlled player ship;
3. advance TrajectoryFollower each fixed step;
4. pass intent through NavigationRuntimeControlBridge into ShipControlState;
5. keep shared physics authoritative;
6. complete/replan/controlled-stop on follower outcomes;
7. integrate DockingRouteRequest::Mode::Automatic;
8. enable START DOCKING only when end-to-end execution is present;
9. live-test automatic approach and docking.

Manual guidance remains available independently.

Commit directly to GitHub; do not provide patch files.
