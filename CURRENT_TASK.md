# CURRENT TASK — M3/M4 physical-authoring replacement

Date: 2026-09-23

Status: **ACTIVE — TYPED PHYSICAL-SOLVE CONTRACT FIRST**

## Why this task is active

The accepted M1 frame/timeline work and the M2 scenario-I/O boundary reach the
real runtime. The remaining high-speed Newtonian failure is not a tuning issue:
the legacy chain authors scalar translational P/V/A before it knows whether the
hull and installed actuators can realize that motion.

Legacy physical-authoring behavior no longer needs to be preserved. It may be
deleted or bypassed as the replacement becomes executable.

## Required solve contract

The physical planner must return exactly one of:

1. one or more bounded candidates for the supplied ship, control law, measured
   state, capability, corridor/terminal request and horizon; or
2. a typed quantitative `InfeasibilityWitness`.

The witness is coordinator feedback. It must distinguish invalid API input,
unsupported law, invalid body frame, missing translation authority, missing
attitude authority, an initial angular state not yet modeled by a candidate
family, insufficient program horizon and numerical/internal failure. Relevant
delta-v, attitude change, minimum timing and usable authority bounds travel
with it.

No result from this stage is accepted merely because it is sampled. Continuous
hull/corridor proof and exact capability/resource proof remain mandatory before
`AcceptedManeuverProgram` publication.

## Current slice

- extend `OrdinaryPhysicalManeuverCompiler::Result` with the typed witness;
- retain pure/value-only API ownership;
- prove that Newtonian main burn does not begin before the required thrust
  attitude is reached;
- prove that a short horizon and missing angular authority fail closed with
  actionable reasons;
- update the purity contract and migration documents;
- run local static gates, then request the focused MinGW64 compiler test.

## Next vertical slices

1. add the persistent coordinator and bounded mutation budget over
   corridor/terminal/speed/arrival-time alternatives;
2. compile literal actuator schedules and preserve full rigid-body initial
   state, including angular velocity;
3. continuously prove the exact candidate against oriented hull and corridor;
4. publish only the proved candidate as `AcceptedManeuverProgram`;
5. replace observe-only execution with literal actuator execution;
6. remove the old translation-first authoring block;
7. add explicit unavoidable-contact mitigation after collision-free search is
   exhausted, without ever accepting an impossible nominal maneuver.

## Target evidence

The aggregate pipeline is expected to remain red until the new vertical path is
connected. The focused compiler test must pass and the existing high-speed test
must not be weakened or hidden.
