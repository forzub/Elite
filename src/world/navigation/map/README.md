# NavigationMap block

`NavigationMap` is the isolated ownership boundary for the active ship-centered
navigation working set.

## Public boundary

The only production-facing API in this block is:

```text
NavigationMap.h
```

Callers may:

```text
publish authoritative DynamicWorldUpdate by value
query a corridor in ship-centered map coordinates
query a local sphere in ship-centered map coordinates
read aggregate statistics
```

Callers do **not** receive references/pointers/views into:

```text
actor storage
spatial cells
prediction cache
working-frame transforms
future GPU buffers/backend objects
```

`NavigationMap` takes ownership of the published snapshot and returns compact
query results by value. This is deliberate: downstream planners consume only the
small relevant subset and cannot start depending on the map's storage layout.

## Coordinate ownership

Input actor positions remain authoritative system/world positions. The update
also supplies the active ship-centered `WorkingFrame` in that same authoritative
coordinate system.

The block performs the transform internally:

```text
authoritative position / velocity / acceleration
                  |
                  v
        NavigationMap working frame
        origin = active ship/domain
        axes   = stable navigation/travel axes
```

Hull roll/pitch/yaw is not the NavigationMap frame.

A Hub remains Hub-local. A future Hub adapter may publish its relevant subset in
an authoritative/source snapshot, but Hub-private storage must never become part
of this API.

## Current implementation wave

The first implementation is a **CPU reference backend** inside
`NavigationMap.cpp`:

```text
owned dynamic actor table
constant-acceleration conservative prediction
sparse 3D cell index
corridor broadphase + exact conservative sphere/segment test
sphere broadphase + exact conservative sphere/sphere test
```

The conservative prediction intentionally matches the GPU benchmark concept:

```text
p1 = p0 + v*T + 0.5*a*T^2
travel_bound = |v|*T + 0.5*|a|*T^2
swept_sphere = sphere(p0, radius + travel_bound)
```

This is a safe first-stage broadphase product, not the final space-time sweep.

The CPU reference exists for three reasons:

1. establish the API before choosing storage/backend ownership;
2. provide a deterministic correctness oracle for the GPU implementation;
3. allow CPU-vs-GPU measurements without changing planner call sites.

A GPU backend must remain behind this same block boundary. OpenGL/GLFW/render
headers are not allowed in `NavigationMap.h`.

## Important limits of this wave

This wave does **not** yet provide the final static free-space/clearance map. It
also does not wire NavigationMap into the live docking planner. Static region
storage, GPU residency and live publication adapters are subsequent waves.

There is intentionally no fixed actor-per-cell correctness cap in the CPU
reference. Out-of-bounds and rejected input records are explicit statistics.

## Tests

Run the standalone behavioral contract from MSYS2 MinGW64:

```bash
bash tests/navigation_map/run_mingw64.sh
```

Run the architectural boundary contract:

```bash
python tests/architecture_contracts/check_navigation_map_boundary.py
```
