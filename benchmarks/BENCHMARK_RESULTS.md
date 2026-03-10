# Mesher benchmark results (standalone)

These results were collected by compiling and running `benchmarks/mesher_benchmark.cpp` directly (no Godot runtime required).

## Environment

- CPU: Intel Xeon Platinum 8370C @ 2.80GHz (3 vCPUs visible in this environment)
- Compiler: `g++`
- Flags: `-O3 -march=native -std=c++17`
- Benchmark scope: `mesh()` only (opaque mask is built once before timing)
- Chunk dimensions: core `62^3`, padded input `64^3`
- Iterations: 5000 per scenario

## Run 1

| case | avg (us) | p50 (us) | p95 (us) | min (us) | max (us) | avg quads |
|---|---:|---:|---:|---:|---:|---:|
| empty | 34.53 | 33.07 | 46.57 | 32.72 | 95.77 | 1 |
| solid(62^3) | 144.10 | 126.90 | 247.62 | 89.39 | 725.69 | 7 |
| terrain(heightmap) | 159.73 | 152.28 | 192.56 | 149.41 | 1292.65 | 7789 |
| noise(50% fill) | 5117.62 | 5054.36 | 5550.75 | 4861.59 | 7359.23 | 297801 |

## Run 2

| case | avg (us) | p50 (us) | p95 (us) | min (us) | max (us) | avg quads |
|---|---:|---:|---:|---:|---:|---:|
| empty | 38.19 | 33.34 | 60.98 | 32.80 | 270.12 | 1 |
| solid(62^3) | 133.31 | 126.24 | 162.12 | 124.52 | 702.48 | 7 |
| terrain(heightmap) | 162.38 | 153.85 | 195.26 | 149.49 | 678.05 | 7789 |
| noise(50% fill) | 5123.14 | 5034.82 | 5776.02 | 4864.38 | 7095.68 | 297801 |

## Notes

- Natural terrain-like data stays around ~150–165 us in this environment.
- Dense random noise is the pathological case because greedy merging opportunities are rare, causing much higher quad counts and runtime.

## 96x96 demo level file benchmark (`levels/demo_terrain_96`)

A benchmark against the actual project map file was added in `benchmarks/level96_benchmark.cpp`.
It times decompression and meshing chunk-by-chunk across all chunks in the level.

- Chunks: 96 x 96 = 9216
- Measured passes: 3 full passes (plus 1 warmup pass)

| metric | value |
|---|---:|
| Decompression avg per chunk (us) | 64.97 |
| Decompression p50/p95 (us) | 63.84 / 89.64 |
| Meshing avg per chunk (us) | 102.56 |
| Meshing p50/p95 (us) | 103.45 / 154.49 |
| Average pass wall time (ms) | 1601.44 |
| Average wall time per chunk (us) | 173.77 |
| Average vertices per chunk | 2109.82 |
