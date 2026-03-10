# TinyVoxel DDGI C++ Project

High-performance C++20 voxel pipeline that:
- Generates tiny voxels procedurally.
- Bakes per-voxel lighting using a custom DDGI voxel cascade light map.
- Meshes with a bitset-driven binary greedy mesher.
- Compresses and stores map data in a compact binary format.
- Renders the final mesh to a `.ppm` image without external graphics dependencies.
- Exposes Godot-ready mesh buffer conversion for future GDExtension integration.

## Why this architecture works for Godot later
The core library (`voxcore`) is engine-agnostic:
- No Godot headers in runtime systems.
- POD-friendly buffers and plain C++ containers.
- Deterministic generation and serialization.
- Separate adapter layer (`GodotInterop`) to map into Godot array formats.

This means you can later wrap `voxcore` inside a GDExtension module without rewriting core data structures.

## Project Layout
- `include/vox/*`: Public engine API.
- `src/*`: Core implementation.
- `app/main.cpp`: End-to-end demo pipeline.
- `tests/roundtrip_tests.cpp`: Basic validation tests.
- `output/`: Generated artifacts (`world_chunk.vxm`, `frame.ppm`).

## Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Run Demo
```bash
./build/vox_demo
```
On Windows with multi-config generators:
```powershell
.\build\Release\vox_demo.exe
```

## Run Tests
```bash
ctest --test-dir build --output-on-failure
```

## Pipeline Overview
1. `TerrainGenerator` creates a dense voxel chunk with caves and emissive pockets.
2. `DDGIVoxelCascade` builds multi-resolution irradiance probe cascades.
3. `bake_light_field` stores quantized per-voxel lighting (`RGB8`).
4. `BinaryGreedyMesher` merges visible faces into large quads using row bitmasks.
5. `ChunkCodec` run-length compresses voxel/material/emission streams.
6. `SoftwareRenderer` rasterizes mesh triangles to `output/frame.ppm`.

## Data Structures (performance-focused)
- **Chunk storage**: fixed-size arrays (`material`, `emission`) + occupancy bitset.
- **Visibility checks**: O(1) occupancy lookup using packed bits.
- **Meshing**: per-plane bit masks + greedy rectangle merge by face-key.
- **Light field**: fixed voxel-aligned RGB8 cache for direct mesher access.
- **Codec**: varint run-length encoding over linearized voxel stream.

## Next steps for GDExtension port
- Add a Godot wrapper class that calls `TerrainGenerator`, `DDGIVoxelCascade`, `BinaryGreedyMesher`.
- Feed `to_godot_buffers(mesh)` output into `ArrayMesh` surfaces.
- Move chunk/world persistence to Godot `FileAccess` while retaining `ChunkCodec` format.
- Optionally move renderer out once Godot viewport rendering is active.
