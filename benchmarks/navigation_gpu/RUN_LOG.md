# Navigation GPU benchmark target-machine run log

**Stage:** `NAV-V2-MAP-2`  
**Date:** 2026-09-16  
**Machine:** user's Windows 10 / MSYS2 MinGW64 target machine

## Attempt 1 — build include-path failure

The benchmark configured but failed to compile because the isolated benchmark used the pre-GLAD-2 include root:

```text
-I D:/__elite/work/glad
fatal error: glad/gl.h: No such file or directory
```

The repository layout is:

```text
glad/include/glad/gl.h
glad/src/gl.c
```

Fix committed on `main`: `benchmarks/navigation_gpu/CMakeLists.txt` now includes `${ELITE_ROOT}/glad/include`.

The architecture contract was also corrected from stale `NAV-V2-GPU-0` to the active `NAV-V2-MAP-2` gate and now pins the GLAD 2 include root.

## Attempt 2 — shader parser failure and wrong GPU selection

After the include-path fix:

```text
NAVIGATION GPU BENCHMARK CONTRACT: PASS
[3/3] Linking CXX executable navigation_gpu_benchmark.exe
[NavGpuBench] OpenGL=4.3 vendor="Intel" renderer="Intel(R) UHD Graphics 630" glsl="4.30 - Build 31.0.101.2137"
[NavGpuBench][FAIL] compute shader compile failed (predict-and-bin): ERROR: 0:105: 'flat' : syntax error syntax error
```

Two independent issues were identified:

1. The shader declares a variable named `flat`. `flat` is a reserved GLSL interpolation qualifier, so the shader source is invalid. Both shader-local uses must be renamed (chosen replacement: `flatIndex`).
2. The OpenGL context was created on Intel UHD Graphics 630 rather than the intended NVIDIA Quadro RTX 3000. Any timing from that adapter would not answer the target backend question for the intended discrete GPU.

A Windows high-performance-GPU preference source is now part of the benchmark executable:

```text
benchmarks/navigation_gpu/GpuPreferenceWin.cpp
```

It exports `NvOptimusEnablement=1` and `AmdPowerXpressRequestHighPerformance=1` before GLFW creates the context. The actual GL vendor/renderer line remains authoritative; if it still reports Intel, the run is not accepted as the Quadro measurement.

## Status

GPU performance remains **UNMEASURED**. Do not compare Intel timings against the CPU benchmark and do not select CPU/GPU/hybrid until the shader is valid and the benchmark reports the intended discrete adapter.
