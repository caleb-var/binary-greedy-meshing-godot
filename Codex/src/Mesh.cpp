#include "vox/Mesh.h"

#include <cmath>

namespace vox {

Vec3f material_albedo(std::uint16_t material) {
    switch (material) {
    case 0:
        return Vec3f{};
    case 1:
        return Vec3f{0.62f, 0.62f, 0.66f}; // stone
    case 2:
        return Vec3f{0.55f, 0.42f, 0.30f}; // dirt
    case 3:
        return Vec3f{0.42f, 0.67f, 0.35f}; // grass
    case 4:
        return Vec3f{1.0f, 0.78f, 0.38f}; // emissive crystal
    case 5:
        return Vec3f{0.78f, 0.72f, 0.48f}; // sand
    default: {
        const float m = static_cast<float>((material * 2654435761u) & 0xffffu) / 65535.0f;
        return Vec3f{0.35f + 0.55f * m, 0.30f + 0.40f * (1.0f - m), 0.25f + 0.45f * (0.5f + 0.5f * std::sin(m * kPi))};
    }
    }
}

std::uint32_t pack_rgba8(const Vec3f& linear_rgb, float alpha) {
    const std::uint32_t r = to_u8(srgb_from_linear(linear_rgb.x));
    const std::uint32_t g = to_u8(srgb_from_linear(linear_rgb.y));
    const std::uint32_t b = to_u8(srgb_from_linear(linear_rgb.z));
    const std::uint32_t a = to_u8(saturate(alpha));
    return r | (g << 8) | (b << 16) | (a << 24);
}

Vec3f unpack_rgba8(std::uint32_t packed_color) {
    const float r = static_cast<float>(packed_color & 0xffu) / 255.0f;
    const float g = static_cast<float>((packed_color >> 8) & 0xffu) / 255.0f;
    const float b = static_cast<float>((packed_color >> 16) & 0xffu) / 255.0f;
    return Vec3f{linear_from_srgb(r), linear_from_srgb(g), linear_from_srgb(b)};
}

} // namespace vox
