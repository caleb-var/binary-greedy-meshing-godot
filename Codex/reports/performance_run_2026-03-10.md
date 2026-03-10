# Codex C++ demo performance run

Date: 2026-03-10

## Build + run commands

```bash
cmake -S Codex -B Codex/build -DCMAKE_BUILD_TYPE=Release
cmake --build Codex/build -j
./Codex/build/vox_demo
```

## Single run result

- Generate: 2.70 ms
- DDGI: 25.36 ms
- Light bake: 2.51 ms
- Meshing: 1.29 ms
- Codec: 0.57 ms
- Render: 139.21 ms

## 8-run aggregate

- Generate: mean 3.17 ms (min 2.67, max 3.57)
- DDGI: mean 31.11 ms (min 28.11, max 33.31)
- Light bake: mean 2.70 ms (min 2.52, max 3.00)
- Meshing: mean 1.44 ms (min 1.22, max 1.66)
- Codec: mean 0.66 ms (min 0.55, max 0.81)
- Render: mean 160.67 ms (min 153.64, max 177.82)

## Quick interpretation

- The software renderer is the dominant stage (~80%+ of frame pipeline time in this run).
- Meshing is already very fast (~1.4 ms mean).
- DDGI is the next major hotspot after rendering.

## Render output

- Generated image: `output/frame.ppm`
- Converted preview: `output/frame.png`
- JPG conversion was attempted but blocked in this environment (no local JPEG encoder tools and package installation is proxy-blocked).
