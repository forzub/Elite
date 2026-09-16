# NavigationMap CPU benchmark

**Stage:** `NAV-V2-MAP-2`  
**Purpose:** measure the isolated CPU reference backend through the public `NavigationMap` API only.

The benchmark deliberately does not inspect `NavigationMap::Impl`, cells, actor storage or prediction caches. It publishes one snapshot and consumes only `queryCorridor()`, `querySphere()` and `stats()`. This keeps the benchmark honest with respect to the module boundary.

It mirrors the existing GPU prototype scenarios:

- `cruise`: 18 km spawn cube, speed <= 250 m/s, acceleration <= 8 m/s^2, 9 km corridor;
- `hub`: 7 km spawn cube, speed <= 120 m/s, acceleration <= 6 m/s^2, 5 km corridor;
- actor counts: 1,000 / 5,000 / 10,000;
- prediction horizon: 3 seconds by default.

Measured separately:

- full snapshot publication / internal rebuild;
- corridor query median and p95;
- sphere/local query median and p95;
- candidate counts;
- cells visited / occupied cells visited / actors examined;
- indexed / out-of-bounds / rejected actor counts.

Build and run from MSYS2 MinGW64:

```bash
bash benchmarks/navigation_map/run_mingw64.sh
```

Optional:

```bash
bash benchmarks/navigation_map/run_mingw64.sh \
  --warmup 5 \
  --iterations 30 \
  --horizon 3 \
  --output navigation_map_cpu_benchmark.csv
```

The CSV path is printed as an absolute path.

These are measurements, not cross-machine pass/fail thresholds. The current Navigation v2 target remains `<0.5 ms` typical main-thread work and `<1.0 ms` normal peak. Snapshot rebuilds may later move off the frame thread even if queries remain CPU-resident.
