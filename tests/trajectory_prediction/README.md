# Shared trajectory predictor tests

This block protects the renderer/server-neutral translational trajectory product.

The predictor receives a system-local kinematic seed, static gravity bodies, an
optional future proper-acceleration program and caller-selected motion envelope.
It returns time-stamped position/velocity/acceleration samples plus separate
proper/gravity acceleration, translational crew load and accumulated proper
`delta-v` diagnostics.

Important boundaries:

- gravity contributes to world acceleration but not to translational crew G;
- proper acceleration and jerk may be limited without clamping physical
  position/velocity directly;
- the caller selects the envelope, so automatic flight may intentionally use a
  higher safe G limit than manual flight without hard-coding that policy in the
  predictor;
- the predictor does not know `RoutePlan`, `SpaceState`, server sessions,
  renderers or map cameras;
- `TrajectoryMapAdapter.h` is the one-way presentation adapter into the existing
  `MapObjectTrajectory` seam.
