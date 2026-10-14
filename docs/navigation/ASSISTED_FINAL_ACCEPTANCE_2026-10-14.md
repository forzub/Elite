# ASSISTED FINAL ACCEPTANCE — 2026-10-14

## Status

**ASSISTED-I: ACCEPTED / FROZEN**

This document is the hard recovery and architectural boundary before dedicated
Newtonian development begins.

Accepted source lineage:

- working branch: `feature/hard-flight-law-isolation-2026-10-14`
- pre-final restore branch: `restore/green-docking-terminal-centering-2026-10-14`
- final accepted restore branch: `restore/assisted-final-accepted-2026-10-14`

## Final acceptance evidence

The final high-speed Assisted stress scenario completed successfully with:

- requests = 1
- failures = 0
- planner_retries = 0
- safety_course_corrections = 1
- emergency_stops = 0
- high_speed_request_mps = 372.329187
- high_speed_route_ready_mps = 397.855088
- status = COMPLETE

The follower entered route execution at approximately 39 m cross-track error and
converged to approximately 6.8–7.1 m before the scenario's first-manoeuvre stop
condition. The previous residual 18–20 m offset is therefore considered fixed.

The high-speed stress scenario intentionally terminates at the first authored
manoeuvre and does not prove BLUE entry itself. BLUE/tunnel physical execution
is covered separately by the green `navigation_v2_tunnel_proving_ground`
contract.

## Accepted Assisted behavior

### Docking entry

1. Clear commanded acceleration without destroying velocity.
2. Inspect the live ballistic course.
3. If blocked, select a safe Assisted course.
4. Turn the hull using ordinary Assisted controls.
5. Let Assisted FA-on physically bend velocity toward the hull/course.
6. Acquire the safe course once.
7. Before capturing a planning snapshot for a new docking request, remove
   requested linear acceleration and damp pitch/yaw/roll rate to <= 0.01 rad/s,
   then hold that settled angular state for 0.20 s. Linear velocity is preserved.
8. Only after that settled state is reached may expensive planning/compilation
   start asynchronously while the craft continues ballistic translation.
9. Preserve the expensive planner backbone.
10. Repair late entry only through a cheap projected connector.
11. Entry rebind is not a new docking request and must not re-run the primary
    safe-course selector.
12. Recoverable handoff drift must not terminal-fail docking.

### Safe-course

- Assisted safe-course uses ordinary physical Assisted controls.
- No direct navigation acceleration/velocity bypass is permitted.
- The current/candidate course is tested with a spatial delta-spot/tube, not an
  infinitesimally thin single ray.
- One primary safety correction is expected for a static request unless a real,
  newly appearing runtime hazard requires a distinct emergency decision.

### Route execution

- The expensive route is immutable backbone geometry.
- The follower owns route execution.
- Assisted uses velocity-course guidance appropriate to FA-on physics.
- Inside the accepted corridor, centering is continuous and gentle.
- Centering uses a deadband near the route centerline but must not double-attenuate
  proportional correction and strand the ship at a visible residual offset.
- Emergency course capture remains separate from normal in-corridor centering.

### Hull / orientation

- Velocity-vector trajectory error is authoritative for route following.
- Hull orientation is an actuator requirement, not the navigation path itself.
- Docking terminal attitude and roll alignment remain explicit end-state goals.

## Frozen architecture

Newtonian work MUST NOT change Assisted semantics to gain implementation reuse.

The following are frozen for Newtonian development:

- Assisted planner behavior
- Assisted docking-entry behavior
- Assisted safe-course physics
- Assisted route interpretation
- Assisted follower semantics
- Assisted centering semantics
- Assisted autopilot control law
- Assisted actuator mapping
- Assisted recovery/handoff behavior

Shared code may remain only where the API and behavior are demonstrably
law-neutral. If Newtonian requires different semantics, duplicate/isolate the
implementation rather than modifying Assisted.

## Hard API boundary

Assisted and Newtonian exchange only explicit law-neutral DTO/API data at shared
boundaries.

Forbidden:

- reaching into the other law's controller state;
- sharing mutable implementation state;
- using runtime law checks to smuggle law-specific behavior through a shared
  helper;
- changing Assisted outputs as collateral damage of Newtonian work.

## Regression gate

Before accepting any Newtonian milestone, the Assisted acceptance subset must
remain green, including at minimum:

- navigation_runtime_control
- docking_status_terminal
- terminal_audio_adapter
- preplanning_safety_course
- in_corridor_centering_guidance
- navigation_v2_tunnel_proving_ground
- ship_propulsion_state

The final high-speed Assisted stress scenario remains the live regression proof.

## Rule from this point forward

**ASSISTED IS DONE.**

Any change to frozen Assisted behavior now requires:

1. a reproducible Assisted-specific defect;
2. a dedicated failing regression;
3. a narrowly scoped fix;
4. proof that Newtonian development is not the reason for the behavior change.

Newtonian development starts from this boundary as an independent control-law
system.
