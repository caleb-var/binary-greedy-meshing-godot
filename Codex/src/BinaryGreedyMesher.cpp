#include "vox/BinaryGreedyMesher.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>

namespace vox {
namespace {

Vec3i axis_direction(int axis, int sign) {
    if (axis == 0) {
        return Vec3i{sign, 0, 0};
    }
    if (axis == 1) {
        return Vec3i{0, sign, 0};
    }
    return Vec3i{0, 0, sign};
}

Vec3i map_plane_to_voxel(int axis, int slice, int u, int v) {
    if (axis == 0) {
        return Vec3i{slice, u, v};
    }
    if (axis == 1) {
        return Vec3i{u, slice, v};
    }
    return Vec3i{u, v, slice};
}

std::uint32_t build_face_key(std::uint16_t material, LightRgb8 light) {
    const std::uint32_t light_565 = static_cast<std::uint32_t>(pack_rgb565(light));
    return static_cast<std::uint32_t>(material) | (light_565 << 16);
}

Vec3f face_color_from_key(std::uint32_t key) {
    const std::uint16_t material = static_cast<std::uint16_t>(key & 0xffffu);
    const std::uint16_t light_packed = static_cast<std::uint16_t>(key >> 16);
    const Vec3f lit = light_to_linear(unpack_rgb565(light_packed));
    Vec3f shaded = material_albedo(material) * lit * 1.12f;
    shaded.x = std::max(0.0f, shaded.x);
    shaded.y = std::max(0.0f, shaded.y);
    shaded.z = std::max(0.0f, shaded.z);
    return shaded;
}

} // namespace

Mesh BinaryGreedyMesher::build(const VoxelChunk& chunk, const VoxelLightField& light_field) const {
    Mesh mesh;
    mesh.vertices.reserve(32768);
    mesh.indices.reserve(65536);

    std::array<std::uint32_t, kChunkArea> keys{};
    std::array<std::uint64_t, kChunkSize> row_masks{};

    for (int axis = 0; axis < 3; ++axis) {
        for (int sign : {-1, 1}) {
            const Vec3i direction = axis_direction(axis, sign);

            for (int slice = 0; slice < kChunkSize; ++slice) {
                keys.fill(0u);
                row_masks.fill(0ull);

                for (int v = 0; v < kChunkSize; ++v) {
                    std::uint64_t row_mask = 0ull;

                    for (int u = 0; u < kChunkSize; ++u) {
                        const Vec3i voxel_pos = map_plane_to_voxel(axis, slice, u, v);
                        if (!chunk.is_solid(voxel_pos.x, voxel_pos.y, voxel_pos.z)) {
                            continue;
                        }

                        const Vec3i neighbor_pos{
                            voxel_pos.x + direction.x,
                            voxel_pos.y + direction.y,
                            voxel_pos.z + direction.z,
                        };

                        if (chunk.is_solid(neighbor_pos.x, neighbor_pos.y, neighbor_pos.z)) {
                            continue;
                        }

                        const int linear_index = VoxelChunk::index_of(voxel_pos.x, voxel_pos.y, voxel_pos.z);
                        const std::uint16_t material = chunk.voxel_linear(linear_index).material;
                        const LightRgb8 light = light_field.light_linear(linear_index);

                        const std::uint32_t face_key = build_face_key(material, light);
                        keys[v * kChunkSize + u] = face_key;
                        row_mask |= (1ull << u);
                    }

                    row_masks[v] = row_mask;
                }

                while (true) {
                    int start_row = -1;
                    for (int row = 0; row < kChunkSize; ++row) {
                        if (row_masks[row] != 0ull) {
                            start_row = row;
                            break;
                        }
                    }

                    if (start_row < 0) {
                        break;
                    }

                    const unsigned int first_bit = std::countr_zero(row_masks[start_row]);
                    const int start_u = static_cast<int>(first_bit);
                    const std::uint32_t face_key = keys[start_row * kChunkSize + start_u];

                    int width = 0;
                    while (start_u + width < kChunkSize) {
                        const std::uint64_t bit = 1ull << (start_u + width);
                        if ((row_masks[start_row] & bit) == 0ull) {
                            break;
                        }
                        if (keys[start_row * kChunkSize + start_u + width] != face_key) {
                            break;
                        }
                        ++width;
                    }

                    if (width <= 0) {
                        row_masks[start_row] &= ~(1ull << start_u);
                        continue;
                    }

                    const std::uint64_t span_mask = ((1ull << width) - 1ull) << start_u;

                    int height = 1;
                    while (start_row + height < kChunkSize) {
                        if ((row_masks[start_row + height] & span_mask) != span_mask) {
                            break;
                        }

                        bool same_key = true;
                        for (int x = 0; x < width; ++x) {
                            if (keys[(start_row + height) * kChunkSize + start_u + x] != face_key) {
                                same_key = false;
                                break;
                            }
                        }

                        if (!same_key) {
                            break;
                        }

                        ++height;
                    }

                    for (int h = 0; h < height; ++h) {
                        row_masks[start_row + h] &= ~span_mask;
                    }

                    emit_quad(mesh, axis, sign, slice, start_u, start_row, width, height, face_key);
                }
            }
        }
    }

    return mesh;
}

void BinaryGreedyMesher::emit_quad(
    Mesh& mesh,
    int axis,
    int sign,
    int slice,
    int u0,
    int v0,
    int width,
    int height,
    std::uint32_t face_key
) const {
    const float plane = static_cast<float>(sign > 0 ? slice + 1 : slice);

    Vec3f base{};
    Vec3f u_axis{};
    Vec3f v_axis{};

    if (axis == 0) {
        base = Vec3f{plane, static_cast<float>(u0), static_cast<float>(v0)};
        u_axis = Vec3f{0.0f, 1.0f, 0.0f};
        v_axis = Vec3f{0.0f, 0.0f, 1.0f};
    } else if (axis == 1) {
        base = Vec3f{static_cast<float>(u0), plane, static_cast<float>(v0)};
        u_axis = Vec3f{1.0f, 0.0f, 0.0f};
        v_axis = Vec3f{0.0f, 0.0f, 1.0f};
    } else {
        base = Vec3f{static_cast<float>(u0), static_cast<float>(v0), plane};
        u_axis = Vec3f{1.0f, 0.0f, 0.0f};
        v_axis = Vec3f{0.0f, 1.0f, 0.0f};
    }

    const Vec3f p0 = base;
    const Vec3f p1 = base + u_axis * static_cast<float>(width);
    const Vec3f p2 = base + u_axis * static_cast<float>(width) + v_axis * static_cast<float>(height);
    const Vec3f p3 = base + v_axis * static_cast<float>(height);

    const std::uint32_t color = pack_rgba8(face_color_from_key(face_key));

    const std::uint32_t start_index = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(Vertex{p0, color});
    mesh.vertices.push_back(Vertex{p1, color});
    mesh.vertices.push_back(Vertex{p2, color});
    mesh.vertices.push_back(Vertex{p3, color});

    if (sign > 0) {
        mesh.indices.push_back(start_index + 0);
        mesh.indices.push_back(start_index + 1);
        mesh.indices.push_back(start_index + 2);
        mesh.indices.push_back(start_index + 0);
        mesh.indices.push_back(start_index + 2);
        mesh.indices.push_back(start_index + 3);
    } else {
        mesh.indices.push_back(start_index + 0);
        mesh.indices.push_back(start_index + 2);
        mesh.indices.push_back(start_index + 1);
        mesh.indices.push_back(start_index + 0);
        mesh.indices.push_back(start_index + 3);
        mesh.indices.push_back(start_index + 2);
    }

    ++mesh.quad_count;
}

} // namespace vox
