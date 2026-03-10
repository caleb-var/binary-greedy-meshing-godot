#pragma once

#include <cstdint>

#include "vox/Math.h"
#include "vox/VoxelChunk.h"

namespace vox {

struct GenerationConfig {
    std::uint32_t seed = 1337;
    float base_height = 12.0f;
    float height_variation = 11.0f;
    float cave_frequency = 0.052f;
    float cave_threshold = 0.72f;
    int bedrock_thickness = 2;
};

class TerrainGenerator {
public:
    explicit TerrainGenerator(GenerationConfig config = {});

    void generate_chunk(VoxelChunk& chunk, const Vec3i& chunk_coordinate) const;

private:
    GenerationConfig config_;
};

} // namespace vox
