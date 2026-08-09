# Interaction Activation Tests

This suite locks the first server-side simulation-activation contract.

It intentionally does **not** change `SimulationMode` yet. It verifies the
cheap broad-phase question that must be correct before activation can control
production simulation:

- object size comes from the existing `LogicalDimensions` contract;
- a large station gets a much larger interaction bound than a small ship;
- relative velocity and closest-point-of-approach can prewarm a pair before a
  fixed distance threshold would notice it;
- diverging entities do not wake just because they are in the same system;
- look-ahead horizon limits how early an interaction wakes;
- gameplay effect range is separate from physical object size;
- radar/sensor visibility is deliberately not part of this subsystem.

The next stage may consume this prediction to drive
`Scheduled/Coarse/Prewarm/Active` transitions and ActiveRegion membership.
