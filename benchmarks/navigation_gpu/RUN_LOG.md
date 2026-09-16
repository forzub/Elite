# Navigation GPU benchmark target-machine run log

**Stage:** `NAV-V2-MAP-2`  
**Date:** 2026-09-16  
**Machine:** user's Windows 10 / MSYS2 MinGW64 target machine

## Attempt 1 — build include-path failure

The benchmark configured but failed to compile because the isolated target used the pre-GLAD-2 include root:

```text
-I D:/__elite/work/glad
fatal error: glad/gl.h: No such file or directory
```

Repository layout:

```text
glad/include/glad/gl.h
glad/src/gl.c
```

Fix: `benchmarks/navigation_gpu/CMakeLists.txt` now exposes `${ELITE_ROOT}/glad/include`. The architecture contract was also corrected from stale `NAV-V2-GPU-0` to `NAV-V2-MAP-2` and pins the GLAD 2 include root.

## Attempt 2 — invalid GLSL identifier and wrong adapter

After the include-path fix:

```text
NAVIGATION GPU BENCHMARK CONTRACT: PASS
[NavGpuBench] OpenGL=4.3 vendor="Intel" renderer="Intel(R) UHD Graphics 630"
[NavGpuBench][FAIL] compute shader compile failed (predict-and-bin):
ERROR: 0:105: 'flat' : syntax error
```

Two independent defects were found:

1. shader-local variable `flat` used the reserved GLSL interpolation qualifier; it was renamed to `flatIndex`;
2. GLFW selected Intel UHD 630 instead of the intended Quadro RTX 3000.

`benchmarks/navigation_gpu/GpuPreferenceWin.cpp` now exports:

```text
NvOptimusEnablement=1
AmdPowerXpressRequestHighPerformance=1
```

The actual GL vendor/renderer line remains authoritative.

The GLSL identifier repair was committed to canonical `main` as:

```text
48c8dbfaefc1a2bda82af762f46a5b38400033ac
fix navigation GPU benchmark GLSL identifiers
```

## Attempt 3 — accepted Quadro default run

The benchmark selected the intended adapter:

```text
OpenGL=4.3
vendor="NVIDIA Corporation"
renderer="Quadro RTX 3000/PCIe/SSE2"
glsl="4.30 NVIDIA via Cg compiler"
horizon_s=3 warmup=5 iterations=30
```

Default results:

```text
scenario actors  bin ms   neighbor ms  total med ms  p95 ms   submit ms
cruise   1000    0.0162   0.5478       0.5642        0.5659   0.0013
cruise   5000    0.0167   0.9298       0.9462        0.9498   0.0014
cruise  10000    0.0194   1.3025       1.3215        1.3230   0.0015
hub      1000    0.0164   0.6536       0.6698        0.6755   0.0013
hub      5000    0.0175   1.7060       1.7228        1.7306   0.0015
hub     10000    0.0119   1.6038       1.6153        3.0371   0.0015
```

All scenarios reported `overflow=0`, `out_of_bounds=0`, `valid=1`, memory about 15.95–16.63 MiB, and fixed readback of 32 bytes. Both 1k scenarios matched the independent CPU correctness reference exactly (`reference_ok=1`).

The single 30-iteration `hub/10k` p95 spike motivated the longer pass before closing the gate.

## Attempt 4 — accepted 100-iteration measurement

Parameters:

```text
horizon_s=3
warmup=10
iterations=100
```

Adapter remained:

```text
vendor="NVIDIA Corporation"
renderer="Quadro RTX 3000/PCIe/SSE2"
```

Measured results:

```text
scenario actors  bin ms   neighbor ms  total med ms  p95 ms   submit ms  neighbor checks
cruise   1000    0.0163   0.5482       0.5646        0.5655   0.0010       8,412
cruise   5000    0.0171   0.9298       0.9466        0.9500   0.0014     213,211
cruise  10000    0.0106   0.6737       0.6840        1.3226   0.0013     855,225
hub      1000    0.0087   0.3350       0.3441        0.3499   0.0005      52,865
hub      5000    0.0091   0.8663       0.8754        0.8806   0.0014   1,353,890
hub     10000    0.0105   1.5991       1.6097        1.6258   0.0014   5,527,327
```

Additional diagnostics:

```text
cruise 10k: pairs=30,684  corridor=59   occupied_cells=8,339
hub    10k: pairs=78,262  corridor=141  occupied_cells=1,719
overflow=0, out_of_bounds=0, valid=1 for every scenario
readback_bytes=32 for every scenario
memory_mib=16.6321 at 10k
reference_ok=1 for both 1k scenarios
```

CSV:

```text
D:\__elite\work\navigation_gpu_benchmark.csv
```

## Gate conclusion

`NAV-V2-MAP-2` is accepted.

The GPU prototype demonstrates that Quadro RTX 3000 can own dynamic actor prediction, binning and all-agent conflict reduction within the current heavy-scene design target at 10k actors in the 100-iteration run: `cruise/10k p95=1.3226 ms`, `hub/10k p95=1.6258 ms`.

The prediction/binning pass itself is approximately `0.01–0.02 ms`; neighbor/conflict reduction is the dominant GPU cost. Dense Hub traffic is therefore the primary future optimization target.

The backend decision is **hybrid**:

- CPU: static free-space/clearance/connectivity/portal topology, sparse global corridor search, precision local search, backend-neutral compact queries;
- GPU: dynamic P/V/A prediction, conservative swept bounds, spatial binning and mass all-agent conflict reduction;
- publication/consumption remains asynchronous and double/triple buffered; no frame-thread dispatch/wait/bulk-readback path is permitted.

CPU and GPU benchmark totals are not treated as equivalent workloads. The decision is based on complementary measured strengths, not a naive total-time winner.

Next stage: `NAV-V2-SPACE-1`.