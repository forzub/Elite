# Critical feature contracts

This block protects user-visible/debug capabilities at their integration seams.
It complements unit/architecture tests; it does not replace them.

Rules:

1. Debug Control settings are auto-discovered from `DebugRenderSettings`. A new
   scalar/color setting must either be wired through the shared codec and HTML
   UI, or be explicitly classified as internal-only by the checker.
2. Critical interactive modes need vertical evidence, not only source-text
   guards. Fast universe therefore boots the real `GameServer` startup scene.
3. Map modes are treated as a feature family: Galaxy -> System -> Detail -> Hub
   must remain present in the application route and behavior tests.
4. `tests/run_all_mingw64.sh` may print `ALL READY BLOCKS PASSED` only while all
   feature-contract evidence remains registered and executable.

When a new critical player/debug feature is added, add one manifest entry and a
behavior/smoke test. Do not rely only on a checkbox, function name, or build.

## Headless authoritative-server boundary

`AssemblyMeshLibrary::get()` is CPU-only. GPU upload is explicitly requested by
render paths through `getGpuReady()`. This keeps `GameServer` construction and
real-scene smoke tests independent of GLFW/OpenGL initialization.
