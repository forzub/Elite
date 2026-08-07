# Shared world-runtime regression gate

Run from the repository root in MSYS2 MinGW64:

```bash
bash tests/world_runtime/run_mingw64.sh
```

The suite locks the first client-world migration contract:

- the server supplies an absolute universe-time anchor and time scale;
- the client advances presentation time locally between server snapshots;
- planetary state is reconstructed from the shared star atlas and the shared
  `CelestialSystemRuntime` implementation;
- `CelestialRuntimeRegistry` evaluates only systems that are requested;
- the old periodic `CelestialSnapshotRequest` network stream may not return.

This gate deliberately does not initialize GLFW or OpenGL. Render-coordinate
contracts are added in the following migration stages when map frame builders
start consuming the client-owned reconstructed world.
