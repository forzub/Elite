# System map behavior regression tests

This executable locks the behavior that must survive the next architecture
steps. It deliberately runs without GLFW, OpenGL, renderer resources or a
server snapshot.

Covered contracts:

- orbit preserves the complete camera pose around the chosen pivot;
- zoom scales target and eye consistently;
- body zoom never crosses the configured safe surface distance;
- rotation pivot priority is body, explicit cell, cursor cell, mouse plane;
- wheel zoom uses only the cursor body or the current camera target;
- refine/coarsen preserves the navigation point;
- an implicit root selection is not an explicit user selection;
- explicit cells resolve to a deterministic terminal descendant;
- body selection clears explicit cell selection and reanchors navigation;
- mode changes are applied only after outgoing-frame capture;
- a fixed mouse/scroll trace produces the same state after every replay.

## Run from MSYS2 MinGW64

From the repository root:

```bash
bash tests/system_map/run_mingw64.sh
```

The script configures an isolated Ninja build under
`build/system_map_behavior_tests`, builds one small executable and runs it with
CTest.

The project already needs GLM and nlohmann/json. The test target uses the same
headers and does not add another dependency or test framework.
