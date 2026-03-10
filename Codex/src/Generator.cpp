#include "vox/Generator.h"

#include <cmath>

namespace vox {
namespace {

std::uint32_t hash_u32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

std::uint32_t hash_coords(int x, int y, int z, std::uint32_t seed) {
    std::uint32_t h = seed;
    h ^= hash_u32(static_cast<std::uint32_t>(x) * 0x9e3779b9u);
    h ^= hash_u32(static_cast<std::uint32_t>(y) * 0x85ebca6bu);
    h ^= hash_u32(static_cast<std::uint32_t>(z) * 0xc2b2ae35u);
    return hash_u32(h);
}

float hash_to_unit(std::uint32_t h) {
    return static_cast<float>(h & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

float lattice_2d(int x, int z, std::uint32_t seed) {
    return hash_to_unit(hash_coords(x, 0, z, seed));
}

float lattice_3d(int x, int y, int z, std::uint32_t seed) {
    return hash_to_unit(hash_coords(x, y, z, seed));
}

float value_noise_2d(float x, float z, std::uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const int z0 = static_cast<int>(std::floor(z));
    const int x1 = x0 + 1;
    const int z1 = z0 + 1;

    const float tx = smoothstep(x - static_cast<float>(x0));
    const float tz = smoothstep(z - static_cast<float>(z0));

    const float n00 = lattice_2d(x0, z0, seed);
    const float n10 = lattice_2d(x1, z0, seed);
    const float n01 = lattice_2d(x0, z1, seed);
    const float n11 = lattice_2d(x1, z1, seed);

    const float a = n00 + (n10 - n00) * tx;
    const float b = n01 + (n11 - n01) * tx;
    return a + (b - a) * tz;
}

float value_noise_3d(float x, float y, float z, std::uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int z0 = static_cast<int>(std::floor(z));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const int z1 = z0 + 1;

    const float tx = smoothstep(x - static_cast<float>(x0));
    const float ty = smoothstep(y - static_cast<float>(y0));
    const float tz = smoothstep(z - static_cast<float>(z0));

    const float n000 = lattice_3d(x0, y0, z0, seed);
    const float n100 = lattice_3d(x1, y0, z0, seed);
    const float n010 = lattice_3d(x0, y1, z0, seed);
    const float n110 = lattice_3d(x1, y1, z0, seed);
    const float n001 = lattice_3d(x0, y0, z1, seed);
    const float n101 = lattice_3d(x1, y0, z1, seed);
    const float n011 = lattice_3d(x0, y1, z1, seed);
    const float n111 = lattice_3d(x1, y1, z1, seed);

    const float x00 = n000 + (n100 - n000) * tx;
    const float x10 = n010 + (n110 - n010) * tx;
    const float x01 = n001 + (n101 - n001) * tx;
    const float x11 = n011 + (n111 - n011) * tx;

    const float y0n = x00 + (x10 - x00) * ty;
    const float y1n = x01 + (x11 - x01) * ty;
    return y0n + (y1n - y0n) * tz;
}

float fractal_2d(float x, float z, std::uint32_t seed, int octaves) {
    float result = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float normalizer = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        result += (value_noise_2d(x * frequency, z * frequency, seed + static_cast<std::uint32_t>(i) * 911u) * 2.0f - 1.0f) * amplitude;
        normalizer += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    return normalizer > 0.0f ? result / normalizer : 0.0f;
}

float fractal_3d(float x, float y, float z, std::uint32_t seed, int octaves) {
    float result = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float normalizer = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        result += (value_noise_3d(x * frequency, y * frequency, z * frequency, seed + static_cast<std::uint32_t>(i) * 577u) * 2.0f - 1.0f) * amplitude;
        normalizer += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    return normalizer > 0.0f ? result / normalizer : 0.0f;
}

} // namespace

TerrainGenerator::TerrainGenerator(GenerationConfig config)
    : config_(config) {}

void TerrainGenerator::generate_chunk(VoxelChunk& chunk, const Vec3i& chunk_coordinate) const {
    chunk.clear();

    const int base_x = chunk_coordinate.x * kChunkSize;
    const int base_y = chunk_coordinate.y * kChunkSize;
    const int base_z = chunk_coordinate.z * kChunkSize;

    for (int z = 0; z < kChunkSize; ++z) {
        for (int x = 0; x < kChunkSize; ++x) {
            const int world_x = base_x + x;
            const int world_z = base_z + z;

            const float low_frequency = fractal_2d(world_x * 0.028f, world_z * 0.028f, config_.seed, 4);
            const float high_frequency = fractal_2d(world_x * 0.11f, world_z * 0.11f, config_.seed + 101u, 2);
            const float ridge = 1.0f - std::fabs(fractal_2d(world_x * 0.041f, world_z * 0.041f, config_.seed + 211u, 3));

            const float terrain_height =
                config_.base_height +
                low_frequency * config_.height_variation +
                high_frequency * 2.0f +
                ridge * 2.8f;

            const int surface_y = static_cast<int>(std::floor(terrain_height));

            for (int y = 0; y < kChunkSize; ++y) {
                const int world_y = base_y + y;

                if (world_y <= config_.bedrock_thickness) {
                    chunk.set_voxel(x, y, z, Voxel{1, 0});
                    continue;
                }

                if (world_y > surface_y) {
                    chunk.set_voxel(x, y, z, Voxel{});
                    continue;
                }

                const float cave_value = fractal_3d(
                    world_x * config_.cave_frequency,
                    world_y * config_.cave_frequency * 1.25f,
                    world_z * config_.cave_frequency,
                    config_.seed + 701u,
                    3
                );

                if (cave_value > config_.cave_threshold && world_y < surface_y - 1) {
                    chunk.set_voxel(x, y, z, Voxel{});
                    continue;
                }

                std::uint16_t material = 1;
                if (world_y >= surface_y) {
                    material = 3;
                } else if (world_y >= surface_y - 2) {
                    material = 2;
                } else if (world_y < 5) {
                    material = 5;
                }

                std::uint8_t emission = 0;
                if (material == 1 && world_y < surface_y - 4) {
                    const std::uint32_t chance = hash_coords(world_x, world_y, world_z, config_.seed + 1703u) & 1023u;
                    if (chance < 3u) {
                        material = 4;
                        emission = 210;
                    }
                }

                chunk.set_voxel(x, y, z, Voxel{material, emission});
            }
        }
    }
}

} // namespace vox
