#pragma once

#include <array>
#include <cstdint>

#include "vox/Math.h"
#include "vox/VoxelChunk.h"

namespace vox {

struct LightRgb8 {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

class VoxelLightField {
public:
    VoxelLightField();

    void clear();

    void set_light(int x, int y, int z, LightRgb8 value);
    [[nodiscard]] LightRgb8 light(int x, int y, int z) const;
    [[nodiscard]] LightRgb8 light_linear(int linear_index) const;

private:
    std::array<LightRgb8, kVoxelCount> lights_{};
};

std::uint16_t pack_rgb565(LightRgb8 value);
LightRgb8 unpack_rgb565(std::uint16_t packed);

Vec3f light_to_linear(LightRgb8 value);
LightRgb8 linear_to_light(const Vec3f& linear_rgb);

} // namespace vox
