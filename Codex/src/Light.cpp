#include "vox/Light.h"

#include <algorithm>
#include <cassert>

namespace vox {

VoxelLightField::VoxelLightField() {
    clear();
}

void VoxelLightField::clear() {
    std::fill(lights_.begin(), lights_.end(), LightRgb8{});
}

void VoxelLightField::set_light(int x, int y, int z, LightRgb8 value) {
    assert(VoxelChunk::in_bounds(x, y, z));
    lights_[VoxelChunk::index_of(x, y, z)] = value;
}

LightRgb8 VoxelLightField::light(int x, int y, int z) const {
    if (!VoxelChunk::in_bounds(x, y, z)) {
        return LightRgb8{};
    }
    return lights_[VoxelChunk::index_of(x, y, z)];
}

LightRgb8 VoxelLightField::light_linear(int linear_index) const {
    assert(linear_index >= 0 && linear_index < kVoxelCount);
    return lights_[linear_index];
}

std::uint16_t pack_rgb565(LightRgb8 value) {
    const std::uint16_t r = static_cast<std::uint16_t>(value.r >> 3);
    const std::uint16_t g = static_cast<std::uint16_t>(value.g >> 2);
    const std::uint16_t b = static_cast<std::uint16_t>(value.b >> 3);
    return static_cast<std::uint16_t>((r << 11) | (g << 5) | b);
}

LightRgb8 unpack_rgb565(std::uint16_t packed) {
    const std::uint8_t r = static_cast<std::uint8_t>(((packed >> 11) & 0x1fu) * 255u / 31u);
    const std::uint8_t g = static_cast<std::uint8_t>(((packed >> 5) & 0x3fu) * 255u / 63u);
    const std::uint8_t b = static_cast<std::uint8_t>((packed & 0x1fu) * 255u / 31u);
    return LightRgb8{r, g, b};
}

Vec3f light_to_linear(LightRgb8 value) {
    return Vec3f{
        linear_from_srgb(static_cast<float>(value.r) / 255.0f),
        linear_from_srgb(static_cast<float>(value.g) / 255.0f),
        linear_from_srgb(static_cast<float>(value.b) / 255.0f),
    };
}

LightRgb8 linear_to_light(const Vec3f& linear_rgb) {
    const float sr = srgb_from_linear(linear_rgb.x);
    const float sg = srgb_from_linear(linear_rgb.y);
    const float sb = srgb_from_linear(linear_rgb.z);
    return LightRgb8{to_u8(sr), to_u8(sg), to_u8(sb)};
}

} // namespace vox
