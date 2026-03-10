#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <cstdint>
#include <vector>

namespace godot {

class BinaryGreedyMesher : public RefCounted {
  GDCLASS(BinaryGreedyMesher, RefCounted)

public:
  BinaryGreedyMesher();

  bool load_level_file(const String &path);
  void clear_level();

  int get_level_size() const;
  int get_chunk_count() const;

  PackedInt64Array mesh_chunk_by_index(int chunk_index);
  PackedInt64Array mesh_chunk_from_rle(const PackedByteArray &rle_data);
  Dictionary mesh_all_loaded_chunks(int thread_count = 0);

protected:
  static void _bind_methods();

private:
  static constexpr int CS = 62;
  static constexpr int CS_P = CS + 2;
  static constexpr int CS_2 = CS * CS;
  static constexpr int CS_P2 = CS_P * CS_P;
  static constexpr int CS_P3 = CS_P * CS_P * CS_P;

  struct ChunkTableEntry {
    uint32_t key = 0;
    uint32_t rle_data_begin = 0;
    uint32_t rle_data_size = 0;
  };

  struct MeshingBuffers {
    std::vector<uint8_t> voxels;
    std::vector<uint64_t> opaque_mask;
    std::vector<uint64_t> face_masks;
    std::vector<uint8_t> forward_merged;
    std::vector<uint8_t> right_merged;
    std::vector<uint64_t> vertices;
    int max_vertices = 4096;

    MeshingBuffers();
    void reset();
  };

  uint8_t level_size_ = 0;
  std::vector<ChunkTableEntry> chunk_table_;
  std::vector<uint8_t> file_buffer_;

  static inline int get_axis_index(int axis, int a, int b, int c);
  static inline uint64_t get_quad(uint64_t x, uint64_t y, uint64_t z, uint64_t w, uint64_t h, uint64_t type);

  static void decompress_to_voxels_and_mask(const uint8_t *rle_data, int rle_size, MeshingBuffers &buffers);
  static void mesh_voxels(MeshingBuffers &buffers);
  static PackedInt64Array to_packed_array(const std::vector<uint64_t> &vertices);
};

} // namespace godot
