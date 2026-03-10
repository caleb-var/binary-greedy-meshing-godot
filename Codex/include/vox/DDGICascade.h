#pragma once

#include <vector>

#include "vox/Light.h"

namespace vox {

struct DDGIConfig {
    int cascade_count = 3;
    int rays_per_probe = 48;
    float base_probe_spacing = 2.0f;
    float max_trace_distance = 28.0f;
    Vec3f sky_color{0.62f, 0.70f, 0.82f};
    Vec3f ambient_floor{0.035f, 0.04f, 0.05f};
    Vec3f sun_direction = normalize(Vec3f{0.55f, -1.0f, 0.25f});
    Vec3f sun_color{1.15f, 1.08f, 0.95f};
};

struct DDGICascadeLevel {
    Vec3i probe_counts{};
    float spacing = 1.0f;
    Vec3f origin{};
    std::vector<Vec3f> irradiance;
};

class DDGIVoxelCascade {
public:
    explicit DDGIVoxelCascade(DDGIConfig config = {});

    void build(const VoxelChunk& chunk);

    [[nodiscard]] Vec3f sample_irradiance(const Vec3f& position) const;
    void bake_light_field(const VoxelChunk& chunk, VoxelLightField& out_light_field) const;

    [[nodiscard]] const std::vector<DDGICascadeLevel>& levels() const noexcept {
        return levels_;
    }

private:
    [[nodiscard]] Vec3f trace_probe(const VoxelChunk& chunk, const Vec3f& probe_position, float step_size, float max_distance) const;
    [[nodiscard]] bool trace_visibility(const VoxelChunk& chunk, const Vec3f& origin, const Vec3f& direction, float max_distance) const;
    [[nodiscard]] Vec3f sample_level(const DDGICascadeLevel& level, const Vec3f& position) const;

    DDGIConfig config_;
    std::vector<Vec3f> ray_directions_;
    std::vector<DDGICascadeLevel> levels_;
};

} // namespace vox
