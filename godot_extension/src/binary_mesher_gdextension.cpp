#include "binary_mesher_gdextension.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

namespace godot {

namespace {
constexpr uint64_t P_MASK = ~(1ull << 63 | 1);

inline uint64_t get_bit_range(uint8_t low, uint8_t high) {
  return ((1ULL << (high - low + 1)) - 1) << low;
}

inline int ctz64(uint64_t value) {
#if defined(_MSC_VER)
  unsigned long pos;
  _BitScanForward64(&pos, value);
  return static_cast<int>(pos);
#else
  return __builtin_ctzll(value);
#endif
}
} // namespace

BinaryGreedyMesher::MeshingBuffers::MeshingBuffers() {
  voxels.resize(CS_P3, 0);
  opaque_mask.resize(CS_P2, 0);
  face_masks.resize(CS_2 * 6, 0);
  forward_merged.resize(CS_2, 0);
  right_merged.resize(CS, 0);
  vertices.resize(max_vertices, 0);
}

void BinaryGreedyMesher::MeshingBuffers::reset() {
  std::fill(voxels.begin(), voxels.end(), 0);
  std::fill(opaque_mask.begin(), opaque_mask.end(), 0);
  std::fill(face_masks.begin(), face_masks.end(), 0);
  std::fill(forward_merged.begin(), forward_merged.end(), 0);
  std::fill(right_merged.begin(), right_merged.end(), 0);
  std::fill(vertices.begin(), vertices.end(), 0);
}

BinaryGreedyMesher::BinaryGreedyMesher() = default;

void BinaryGreedyMesher::_bind_methods() {
  ClassDB::bind_method(D_METHOD("load_level_file", "path"), &BinaryGreedyMesher::load_level_file);
  ClassDB::bind_method(D_METHOD("clear_level"), &BinaryGreedyMesher::clear_level);
  ClassDB::bind_method(D_METHOD("get_level_size"), &BinaryGreedyMesher::get_level_size);
  ClassDB::bind_method(D_METHOD("get_chunk_count"), &BinaryGreedyMesher::get_chunk_count);
  ClassDB::bind_method(D_METHOD("mesh_chunk_by_index", "chunk_index"), &BinaryGreedyMesher::mesh_chunk_by_index);
  ClassDB::bind_method(D_METHOD("mesh_chunk_from_rle", "rle_data"), &BinaryGreedyMesher::mesh_chunk_from_rle);
  ClassDB::bind_method(D_METHOD("mesh_all_loaded_chunks", "thread_count"), &BinaryGreedyMesher::mesh_all_loaded_chunks, DEFVAL(0));
}

void BinaryGreedyMesher::clear_level() {
  level_size_ = 0;
  chunk_table_.clear();
  file_buffer_.clear();
}

bool BinaryGreedyMesher::load_level_file(const String &path) {
  clear_level();

  Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
  if (file.is_null()) {
    return false;
  }

  const uint64_t file_size = file->get_length();
  if (file_size < 1) {
    return false;
  }

  file_buffer_.resize(static_cast<size_t>(file_size));
  PackedByteArray bytes = file->get_buffer(static_cast<int64_t>(file_size));
  std::memcpy(file_buffer_.data(), bytes.ptr(), static_cast<size_t>(bytes.size()));

  level_size_ = file_buffer_[0];
  const uint64_t table_len = static_cast<uint64_t>(level_size_) * static_cast<uint64_t>(level_size_);
  const uint64_t table_bytes = table_len * sizeof(ChunkTableEntry);

  if (file_buffer_.size() < 1 + table_bytes) {
    clear_level();
    return false;
  }

  chunk_table_.resize(static_cast<size_t>(table_len));
  std::memcpy(chunk_table_.data(), file_buffer_.data() + 1, static_cast<size_t>(table_bytes));
  return true;
}

int BinaryGreedyMesher::get_level_size() const {
  return static_cast<int>(level_size_);
}

int BinaryGreedyMesher::get_chunk_count() const {
  return static_cast<int>(chunk_table_.size());
}

PackedInt64Array BinaryGreedyMesher::mesh_chunk_by_index(int chunk_index) {
  if (chunk_index < 0 || chunk_index >= static_cast<int>(chunk_table_.size())) {
    return PackedInt64Array();
  }

  const ChunkTableEntry &entry = chunk_table_[static_cast<size_t>(chunk_index)];
  const uint64_t begin = static_cast<uint64_t>(entry.rle_data_begin);
  const uint64_t size = static_cast<uint64_t>(entry.rle_data_size);
  if (begin + size > file_buffer_.size()) {
    return PackedInt64Array();
  }

  MeshingBuffers buffers;
  decompress_to_voxels_and_mask(file_buffer_.data() + begin, static_cast<int>(size), buffers);
  mesh_voxels(buffers);
  return to_packed_array(buffers.vertices);
}

PackedInt64Array BinaryGreedyMesher::mesh_chunk_from_rle(const PackedByteArray &rle_data) {
  MeshingBuffers buffers;
  decompress_to_voxels_and_mask(rle_data.ptr(), rle_data.size(), buffers);
  mesh_voxels(buffers);
  return to_packed_array(buffers.vertices);
}

Dictionary BinaryGreedyMesher::mesh_all_loaded_chunks(int thread_count) {
  Dictionary result;
  if (chunk_table_.empty()) {
    result["chunks"] = 0;
    result["total_quads"] = 0;
    result["elapsed_ms"] = 0.0;
    return result;
  }

  const int workers = thread_count > 0 ? thread_count : std::max(1u, std::thread::hardware_concurrency());
  std::atomic<int> next_chunk = 0;
  std::atomic<uint64_t> total_quads = 0;

  auto start = std::chrono::high_resolution_clock::now();
  std::vector<std::thread> threads;
  threads.reserve(static_cast<size_t>(workers));

  for (int worker = 0; worker < workers; ++worker) {
    threads.emplace_back([&]() {
      MeshingBuffers buffers;
      while (true) {
        const int chunk_index = next_chunk.fetch_add(1);
        if (chunk_index >= static_cast<int>(chunk_table_.size())) {
          break;
        }

        const ChunkTableEntry &entry = chunk_table_[static_cast<size_t>(chunk_index)];
        const uint64_t begin = static_cast<uint64_t>(entry.rle_data_begin);
        const uint64_t size = static_cast<uint64_t>(entry.rle_data_size);
        if (begin + size > file_buffer_.size()) {
          continue;
        }

        buffers.reset();
        decompress_to_voxels_and_mask(file_buffer_.data() + begin, static_cast<int>(size), buffers);
        mesh_voxels(buffers);
        total_quads.fetch_add(static_cast<uint64_t>(buffers.vertices.size()));
      }
    });
  }

  for (std::thread &thread : threads) {
    thread.join();
  }

  auto end = std::chrono::high_resolution_clock::now();
  const std::chrono::duration<double, std::milli> elapsed = end - start;

  result["chunks"] = static_cast<int>(chunk_table_.size());
  result["total_quads"] = static_cast<int64_t>(total_quads.load());
  result["elapsed_ms"] = elapsed.count();
  return result;
}

inline int BinaryGreedyMesher::get_axis_index(int axis, int a, int b, int c) {
  if (axis == 0) {
    return b + (a * CS_P) + (c * CS_P2);
  }
  if (axis == 1) {
    return b + (c * CS_P) + (a * CS_P2);
  }
  return c + (a * CS_P) + (b * CS_P2);
}

inline uint64_t BinaryGreedyMesher::get_quad(uint64_t x, uint64_t y, uint64_t z, uint64_t w, uint64_t h, uint64_t type) {
  return (type << 32) | (h << 24) | (w << 18) | (z << 12) | (y << 6) | x;
}

void BinaryGreedyMesher::decompress_to_voxels_and_mask(const uint8_t *rle_data, int rle_size, MeshingBuffers &buffers) {
  uint8_t *u_p = buffers.voxels.data();

  int opaque_mask_index = 0;
  int opaque_mask_bit_index = 0;

  for (int i = 0; i < rle_size; i += 2) {
    const uint8_t type = rle_data[i];
    const uint8_t len = rle_data[i + 1];

    std::memset(u_p, type, len);

    int remaining_length = len;
    while (remaining_length) {
      const int remaining_bits = 64 - opaque_mask_bit_index;
      if (remaining_length < remaining_bits) {
        if (type) {
          buffers.opaque_mask[opaque_mask_index] |= get_bit_range(static_cast<uint8_t>(opaque_mask_bit_index), static_cast<uint8_t>(opaque_mask_bit_index + remaining_length - 1));
        }
        opaque_mask_bit_index += remaining_length;
        remaining_length = 0;
      } else if (remaining_length >= 64 && opaque_mask_bit_index == 0) {
        const int count = remaining_length / 64;
        if (type) {
          std::fill_n(buffers.opaque_mask.begin() + opaque_mask_index, count, ~0ull);
        }
        opaque_mask_index += count;
        remaining_length -= count * 64;
      } else {
        if (type) {
          buffers.opaque_mask[opaque_mask_index] |= get_bit_range(static_cast<uint8_t>(opaque_mask_bit_index), 63);
        }
        remaining_length -= remaining_bits;
        opaque_mask_index++;
        opaque_mask_bit_index = 0;
      }
    }

    u_p += len;
  }
}

void BinaryGreedyMesher::mesh_voxels(MeshingBuffers &buffers) {
  buffers.vertices.clear();

  auto push_quad = [&](uint64_t quad) {
    buffers.vertices.push_back(quad);
  };

  uint64_t *opaque_mask = buffers.opaque_mask.data();
  uint64_t *face_masks = buffers.face_masks.data();
  uint8_t *forward_merged = buffers.forward_merged.data();
  uint8_t *right_merged = buffers.right_merged.data();

  for (int a = 1; a < CS_P - 1; a++) {
    const int a_cs_p = a * CS_P;

    for (int b = 1; b < CS_P - 1; b++) {
      const uint64_t column_bits = opaque_mask[(a * CS_P) + b] & P_MASK;
      const int ba_index = (b - 1) + (a - 1) * CS;
      const int ab_index = (a - 1) + (b - 1) * CS;

      face_masks[ba_index + 0 * CS_2] = (column_bits & ~opaque_mask[a_cs_p + CS_P + b]) >> 1;
      face_masks[ba_index + 1 * CS_2] = (column_bits & ~opaque_mask[a_cs_p - CS_P + b]) >> 1;
      face_masks[ab_index + 2 * CS_2] = (column_bits & ~opaque_mask[a_cs_p + (b + 1)]) >> 1;
      face_masks[ab_index + 3 * CS_2] = (column_bits & ~opaque_mask[a_cs_p + (b - 1)]) >> 1;
      face_masks[ba_index + 4 * CS_2] = column_bits & ~(opaque_mask[a_cs_p + b] >> 1);
      face_masks[ba_index + 5 * CS_2] = column_bits & ~(opaque_mask[a_cs_p + b] << 1);
    }
  }

  for (int face = 0; face < 4; face++) {
    const int axis = face / 2;

    for (int layer = 0; layer < CS; layer++) {
      const int bits_location = layer * CS + face * CS_2;

      for (int forward = 0; forward < CS; forward++) {
        uint64_t bits_here = face_masks[forward + bits_location];
        if (bits_here == 0) {
          continue;
        }

        const uint64_t bits_next = forward + 1 < CS ? face_masks[(forward + 1) + bits_location] : 0;
        uint8_t right_merged_local = 1;
        while (bits_here) {
          const int bit_pos = ctz64(bits_here);
          const uint8_t type = buffers.voxels[get_axis_index(axis, forward + 1, bit_pos + 1, layer + 1)];
          uint8_t &forward_merged_ref = forward_merged[bit_pos];

          if ((bits_next >> bit_pos & 1) && type == buffers.voxels[get_axis_index(axis, forward + 2, bit_pos + 1, layer + 1)]) {
            forward_merged_ref++;
            bits_here &= ~(1ull << bit_pos);
            continue;
          }

          for (int right = bit_pos + 1; right < CS; right++) {
            if (!(bits_here >> right & 1) || forward_merged_ref != forward_merged[right] || type != buffers.voxels[get_axis_index(axis, forward + 1, right + 1, layer + 1)]) {
              break;
            }
            forward_merged[right] = 0;
            right_merged_local++;
          }
          bits_here &= ~((1ull << (bit_pos + right_merged_local)) - 1);

          const uint8_t mesh_front = forward - forward_merged_ref;
          const uint8_t mesh_left = bit_pos;
          const uint8_t mesh_up = layer + (~face & 1);
          const uint8_t mesh_width = right_merged_local;
          const uint8_t mesh_length = forward_merged_ref + 1;

          forward_merged_ref = 0;
          right_merged_local = 1;

          uint64_t quad;
          switch (face) {
            case 0:
            case 1:
              quad = get_quad(mesh_front + (face == 1 ? mesh_length : 0), mesh_up, mesh_left, mesh_length, mesh_width, type);
              break;
            case 2:
            case 3:
            default:
              quad = get_quad(mesh_up, mesh_front + (face == 2 ? mesh_length : 0), mesh_left, mesh_length, mesh_width, type);
              break;
          }

          push_quad(quad);
        }
      }
    }
  }

  for (int face = 4; face < 6; face++) {
    const int axis = face / 2;

    for (int forward = 0; forward < CS; forward++) {
      const int bits_location = forward * CS + face * CS_2;
      const int bits_forward_location = (forward + 1) * CS + face * CS_2;

      for (int right = 0; right < CS; right++) {
        uint64_t bits_here = face_masks[right + bits_location];
        if (bits_here == 0) {
          continue;
        }

        const uint64_t bits_forward = forward < CS - 1 ? face_masks[right + bits_forward_location] : 0;
        const uint64_t bits_right = right < CS - 1 ? face_masks[right + 1 + bits_location] : 0;
        const int right_cs = right * CS;

        while (bits_here) {
          const int bit_pos = ctz64(bits_here);
          bits_here &= ~(1ull << bit_pos);

          const uint8_t type = buffers.voxels[get_axis_index(axis, right + 1, forward + 1, bit_pos)];
          uint8_t &forward_merged_ref = forward_merged[right_cs + (bit_pos - 1)];
          uint8_t &right_merged_ref = right_merged[bit_pos - 1];

          if (right_merged_ref == 0 && (bits_forward >> bit_pos & 1) && type == buffers.voxels[get_axis_index(axis, right + 1, forward + 2, bit_pos)]) {
            forward_merged_ref++;
            continue;
          }

          if ((bits_right >> bit_pos & 1) && forward_merged_ref == forward_merged[(right_cs + CS) + (bit_pos - 1)] && type == buffers.voxels[get_axis_index(axis, right + 2, forward + 1, bit_pos)]) {
            forward_merged_ref = 0;
            right_merged_ref++;
            continue;
          }

          const uint8_t mesh_left = right - right_merged_ref;
          const uint8_t mesh_front = forward - forward_merged_ref;
          const uint8_t mesh_up = bit_pos - 1 + (~face & 1);
          const uint8_t mesh_width = 1 + right_merged_ref;
          const uint8_t mesh_length = 1 + forward_merged_ref;

          forward_merged_ref = 0;
          right_merged_ref = 0;

          const uint64_t quad = get_quad(mesh_left + (face == 4 ? mesh_width : 0), mesh_front, mesh_up, mesh_width, mesh_length, type);
          push_quad(quad);
        }
      }
    }
  }
}

PackedInt64Array BinaryGreedyMesher::to_packed_array(const std::vector<uint64_t> &vertices) {
  PackedInt64Array packed;
  packed.resize(static_cast<int64_t>(vertices.size()));
  for (int64_t i = 0; i < static_cast<int64_t>(vertices.size()); ++i) {
    packed.set(i, static_cast<int64_t>(vertices[static_cast<size_t>(i)]));
  }
  return packed;
}

} // namespace godot
