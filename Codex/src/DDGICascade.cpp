#include "vox/DDGICascade.h"

#include <algorithm>
#include <cmath>

#include "vox/Mesh.h"

namespace vox {
namespace {

std::vector<Vec3f> fibonacci_directions(int count) {
    std::vector<Vec3f> dirs;
    dirs.reserve(static_cast<std::size_t>(count));

    const float golden_angle = kPi * (3.0f - std::sqrt(5.0f));
    for (int i = 0; i < count; ++i) {
        const float t = (i + 0.5f) / static_cast<float>(count);
        const float y = 1.0f - 2.0f * t;
        const float radius = std::sqrt(std::max(0.0f, 1.0f - y * y));
        const float theta = golden_angle * static_cast<float>(i);
        dirs.push_back(normalize(Vec3f{std::cos(theta) * radius, y, std::sin(theta) * radius}));
    }

    return dirs;
}

std::size_t level_index(const DDGICascadeLevel& level, int x, int y, int z) {
    return static_cast<std::size_t>(x + y * level.probe_counts.x + z * level.probe_counts.x * level.probe_counts.y);
}

} // namespace

DDGIVoxelCascade::DDGIVoxelCascade(DDGIConfig config)
    : config_(config)
    , ray_directions_(fibonacci_directions(std::max(8, config.rays_per_probe))) {}

void DDGIVoxelCascade::build(const VoxelChunk& chunk) {
    levels_.clear();
    levels_.reserve(static_cast<std::size_t>(std::max(1, config_.cascade_count)));

    for (int cascade = 0; cascade < std::max(1, config_.cascade_count); ++cascade) {
        DDGICascadeLevel level;
        level.spacing = config_.base_probe_spacing * static_cast<float>(1 << cascade);
        level.origin = Vec3f{};

        const int probes_per_axis = std::max(
            2,
            static_cast<int>(std::ceil((static_cast<float>(kChunkSize) + level.spacing) / level.spacing))
        );
        level.probe_counts = Vec3i{probes_per_axis, probes_per_axis, probes_per_axis};

        const std::size_t probe_count = static_cast<std::size_t>(probes_per_axis) * probes_per_axis * probes_per_axis;
        level.irradiance.resize(probe_count);

        const float step = std::max(0.45f, level.spacing * 0.35f);
        const float trace_distance = std::min(config_.max_trace_distance, level.spacing * 9.0f);

        for (int z = 0; z < probes_per_axis; ++z) {
            for (int y = 0; y < probes_per_axis; ++y) {
                for (int x = 0; x < probes_per_axis; ++x) {
                    const Vec3f probe_position{
                        x * level.spacing,
                        y * level.spacing,
                        z * level.spacing,
                    };
                    level.irradiance[level_index(level, x, y, z)] = trace_probe(chunk, probe_position, step, trace_distance);
                }
            }
        }

        levels_.push_back(std::move(level));
    }
}

Vec3f DDGIVoxelCascade::trace_probe(
    const VoxelChunk& chunk,
    const Vec3f& probe_position,
    float step_size,
    float max_distance
) const {
    Vec3f total = config_.ambient_floor;

    for (const Vec3f& ray_direction : ray_directions_) {
        Vec3f contribution{};
        bool terminated = false;

        for (float t = step_size; t <= max_distance; t += step_size) {
            const Vec3f sample_point = probe_position + ray_direction * t;
            const int sx = static_cast<int>(std::floor(sample_point.x));
            const int sy = static_cast<int>(std::floor(sample_point.y));
            const int sz = static_cast<int>(std::floor(sample_point.z));

            if (!VoxelChunk::in_bounds(sx, sy, sz)) {
                if (ray_direction.y > 0.0f) {
                    contribution += config_.sky_color * ray_direction.y;
                }
                terminated = true;
                break;
            }

            if (chunk.is_solid(sx, sy, sz)) {
                const Voxel hit = chunk.voxel(sx, sy, sz);
                const float hit_boost = std::max(0.05f, 1.0f - (t / max_distance));

                if (hit.emission > 0) {
                    const float e = static_cast<float>(hit.emission) / 255.0f;
                    contribution += Vec3f{1.0f, 0.72f, 0.35f} * (0.9f + 2.6f * e) * hit_boost;
                }

                contribution += material_albedo(hit.material) * 0.09f * hit_boost;
                terminated = true;
                break;
            }
        }

        if (!terminated) {
            contribution += config_.ambient_floor;
        }

        total += contribution;
    }

    const Vec3f to_sun = normalize(Vec3f{-config_.sun_direction.x, -config_.sun_direction.y, -config_.sun_direction.z});
    if (trace_visibility(chunk, probe_position + Vec3f{0.0f, 0.25f, 0.0f}, to_sun, config_.max_trace_distance)) {
        total += config_.sun_color * 0.38f;
    }

    return total / static_cast<float>(ray_directions_.size());
}

bool DDGIVoxelCascade::trace_visibility(
    const VoxelChunk& chunk,
    const Vec3f& origin,
    const Vec3f& direction,
    float max_distance
) const {
    constexpr float kVisibilityStep = 0.6f;

    for (float t = 0.7f; t <= max_distance; t += kVisibilityStep) {
        const Vec3f p = origin + direction * t;
        const int x = static_cast<int>(std::floor(p.x));
        const int y = static_cast<int>(std::floor(p.y));
        const int z = static_cast<int>(std::floor(p.z));

        if (!VoxelChunk::in_bounds(x, y, z)) {
            return true;
        }

        if (chunk.is_solid(x, y, z)) {
            return false;
        }
    }

    return true;
}

Vec3f DDGIVoxelCascade::sample_level(const DDGICascadeLevel& level, const Vec3f& position) const {
    const auto sample_clamped = [&](int x, int y, int z) {
        const int cx = std::clamp(x, 0, level.probe_counts.x - 1);
        const int cy = std::clamp(y, 0, level.probe_counts.y - 1);
        const int cz = std::clamp(z, 0, level.probe_counts.z - 1);
        return level.irradiance[level_index(level, cx, cy, cz)];
    };

    const Vec3f local = (position - level.origin) / level.spacing;
    int x0 = static_cast<int>(std::floor(local.x));
    int y0 = static_cast<int>(std::floor(local.y));
    int z0 = static_cast<int>(std::floor(local.z));

    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const int z1 = z0 + 1;

    const float tx = saturate(local.x - static_cast<float>(x0));
    const float ty = saturate(local.y - static_cast<float>(y0));
    const float tz = saturate(local.z - static_cast<float>(z0));

    const Vec3f c000 = sample_clamped(x0, y0, z0);
    const Vec3f c100 = sample_clamped(x1, y0, z0);
    const Vec3f c010 = sample_clamped(x0, y1, z0);
    const Vec3f c110 = sample_clamped(x1, y1, z0);
    const Vec3f c001 = sample_clamped(x0, y0, z1);
    const Vec3f c101 = sample_clamped(x1, y0, z1);
    const Vec3f c011 = sample_clamped(x0, y1, z1);
    const Vec3f c111 = sample_clamped(x1, y1, z1);

    const Vec3f x00 = lerp(c000, c100, tx);
    const Vec3f x10 = lerp(c010, c110, tx);
    const Vec3f x01 = lerp(c001, c101, tx);
    const Vec3f x11 = lerp(c011, c111, tx);

    const Vec3f y00 = lerp(x00, x10, ty);
    const Vec3f y01 = lerp(x01, x11, ty);
    return lerp(y00, y01, tz);
}

Vec3f DDGIVoxelCascade::sample_irradiance(const Vec3f& position) const {
    if (levels_.empty()) {
        return config_.ambient_floor;
    }

    Vec3f accumulated{};
    float weight_sum = 0.0f;

    for (const DDGICascadeLevel& level : levels_) {
        const float weight = 1.0f / std::max(0.01f, level.spacing);
        accumulated += sample_level(level, position) * weight;
        weight_sum += weight;
    }

    if (weight_sum <= 0.0f) {
        return config_.ambient_floor;
    }
    return accumulated / weight_sum;
}

void DDGIVoxelCascade::bake_light_field(const VoxelChunk& chunk, VoxelLightField& out_light_field) const {
    out_light_field.clear();

    for (int z = 0; z < kChunkSize; ++z) {
        for (int y = 0; y < kChunkSize; ++y) {
            for (int x = 0; x < kChunkSize; ++x) {
                if (!chunk.is_solid(x, y, z)) {
                    continue;
                }

                const Voxel voxel_value = chunk.voxel(x, y, z);
                const Vec3f irradiance = sample_irradiance(Vec3f{
                    x + 0.5f,
                    y + 0.5f,
                    z + 0.5f,
                });

                Vec3f lit = material_albedo(voxel_value.material) * irradiance * 1.35f;

                if (voxel_value.emission > 0) {
                    const float emission_strength = static_cast<float>(voxel_value.emission) / 255.0f;
                    lit += Vec3f{1.0f, 0.75f, 0.38f} * (1.3f + 2.8f * emission_strength);
                }

                out_light_field.set_light(x, y, z, linear_to_light(lit));
            }
        }
    }
}

} // namespace vox
