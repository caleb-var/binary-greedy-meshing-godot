#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vox/Math.h"

namespace vox {

struct Vertex {
    Vec3f position{};
    std::uint32_t color_rgba8 = 0xffffffffu;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::size_t quad_count = 0;

    void clear() {
        vertices.clear();
        indices.clear();
        quad_count = 0;
    }
};

Vec3f material_albedo(std::uint16_t material);
std::uint32_t pack_rgba8(const Vec3f& linear_rgb, float alpha = 1.0f);
Vec3f unpack_rgba8(std::uint32_t packed_color);

} // namespace vox
