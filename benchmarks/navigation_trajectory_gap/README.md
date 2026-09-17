# Navigation trajectory bounded-gap benchmark

This harness measures only the **bounded gap-candidate builder** used after ordinary local avoidance has already reduced the problem to one primary conflict plus a local neighbor set.

It does not run global search, `NavigationMap` broadphase, oriented passage projection, or full 6DoF trajectory solving.

## Scenarios

```text
reject_16/64/256/1024
    every neighbor is irrelevant / outside the bounded corridor
    measures the cheap rejection scan

top8_16/64/256/1024
    many neighbors are plausible transverse gap partners
    builder continuously maintains the deterministic best 8 candidates
```

The builder contract is intentionally:

```text
O(local_neighbors * 8)
```

not all-pairs `O(N^2)`. The hard candidate cap is `8`.

`1024` local neighbors is a deliberate stress ceiling, not an expected normal precision-fallback input.

## Run

From MSYS2 MinGW64:

```bash
cd /d/__elite/work
python tests/architecture_contracts/check_navigation_trajectory_gap.py
bash tests/navigation_trajectory/run_mingw64.sh
bash benchmarks/navigation_trajectory_gap/run_mingw64.sh
```

Optional:

```bash
bash benchmarks/navigation_trajectory_gap/run_mingw64.sh --warmup 5 --iterations 30
```

## Interpretation

This is a fallback-path microbenchmark. The ordinary clear/adjusted flight path must not execute this builder at all.

Target-machine evidence should be read relative to the existing local CPU budget:

```text
<0.5 ms typical
<1.0 ms normal peak
```

The benchmark is expected to stay far below those numbers for realistic reduced-neighbor counts. If only the artificial 1024-neighbor case becomes material, reduce/cap the neighbor input before changing the constant-size precision geometry.
