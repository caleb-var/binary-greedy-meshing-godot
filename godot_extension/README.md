# Godot C++ GDExtension port (starter)

This folder contains the initial C++ GDExtension port of the upstream binary greedy mesher.

## What is ported

- The same padded chunk dimensions (`62^3` meshable cells from `64^3` input with neighbor padding).
- The same acceleration data layout used by the original implementation:
  - `opaque_mask` (`64*64` columns of `uint64_t` bitfields)
  - `face_masks` (`6 * 62*62` bitfield planes)
  - merge helpers (`forward_merged`, `right_merged`)
- RLE decompression directly into voxel and occupancy structures.
- The same greedy quad merge loops and quad packing (`uint64_t` per quad).
- Multi-threaded full-level meshing entrypoint for large map benchmarking.

## Exposed Godot class

`BinaryGreedyMesher` (`RefCounted`) exposes:

- `bool load_level_file(path: String)`
- `void clear_level()`
- `int get_level_size()`
- `int get_chunk_count()`
- `PackedInt64Array mesh_chunk_by_index(chunk_index: int)`
- `PackedInt64Array mesh_chunk_from_rle(rle_data: PackedByteArray)`
- `Dictionary mesh_all_loaded_chunks(thread_count: int = 0)`

`mesh_all_loaded_chunks` returns:

- `chunks`: total chunk count
- `total_quads`: summed quad count from all chunks
- `elapsed_ms`: total meshing wall time in milliseconds

## Next steps

1. Add a Godot-side renderer that unpacks and draws the packed quads (MultiMesh or RenderingDevice path).
2. Add regression tests that compare packed quad output against the original repo executable for identical input chunks.
3. Add a build script (`SConstruct`/CMake) wired to your `godot-cpp` checkout and produce `.gdextension` config files.
