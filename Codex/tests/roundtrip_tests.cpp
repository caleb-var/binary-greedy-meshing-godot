#include <iostream>

#include "vox/BinaryGreedyMesher.h"
#include "vox/ChunkCodec.h"
#include "vox/DDGICascade.h"
#include "vox/Generator.h"

namespace {

int g_failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        ++g_failures;
        std::cerr << "[FAIL] " << message << "\n";
    }
}

} // namespace

int main() {
    {
        vox::VoxelChunk chunk;
        chunk.set_voxel(0, 0, 0, vox::Voxel{1, 0});
        chunk.set_voxel(1, 0, 0, vox::Voxel{2, 0});
        chunk.set_voxel(1, 1, 0, vox::Voxel{4, 180});
        chunk.set_voxel(6, 7, 8, vox::Voxel{3, 0});

        const auto encoded = vox::ChunkCodec::encode(chunk);
        vox::VoxelChunk decoded;
        const bool ok = vox::ChunkCodec::decode(encoded, decoded);

        expect(ok, "Codec should decode encoded chunk");
        expect(chunk.equals(decoded), "Codec roundtrip should preserve voxel data");
    }

    {
        vox::VoxelChunk chunk;
        chunk.set_voxel(10, 10, 10, vox::Voxel{1, 0});

        vox::VoxelLightField lights;
        lights.set_light(10, 10, 10, vox::LightRgb8{255, 255, 255});

        vox::BinaryGreedyMesher mesher;
        const vox::Mesh mesh = mesher.build(chunk, lights);

        expect(mesh.quad_count == 6, "Single voxel should produce exactly 6 quads");
        expect(mesh.indices.size() == 36, "Single voxel should produce 12 triangles");
    }

    {
        vox::VoxelChunk chunk;
        vox::TerrainGenerator generator;
        generator.generate_chunk(chunk, vox::Vec3i{0, 0, 0});

        vox::DDGIVoxelCascade ddgi;
        ddgi.build(chunk);

        vox::VoxelLightField field;
        ddgi.bake_light_field(chunk, field);

        expect(chunk.solid_count() > 0, "Generated chunk should not be empty");

        bool has_lit_voxel = false;
        for (int z = 0; z < vox::kChunkSize && !has_lit_voxel; ++z) {
            for (int y = 0; y < vox::kChunkSize && !has_lit_voxel; ++y) {
                for (int x = 0; x < vox::kChunkSize; ++x) {
                    if (!chunk.is_solid(x, y, z)) {
                        continue;
                    }
                    const vox::LightRgb8 light = field.light(x, y, z);
                    if (light.r > 0 || light.g > 0 || light.b > 0) {
                        has_lit_voxel = true;
                        break;
                    }
                }
            }
        }

        expect(has_lit_voxel, "At least one solid voxel should have non-zero baked light");
    }

    if (g_failures == 0) {
        std::cout << "All tests passed\n";
        return 0;
    }

    std::cerr << g_failures << " test(s) failed\n";
    return 1;
}
