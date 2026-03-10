#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <vector>

#include "vox/BinaryGreedyMesher.h"
#include "vox/ChunkCodec.h"
#include "vox/DDGICascade.h"
#include "vox/Generator.h"
#include "vox/GodotInterop.h"
#include "vox/SoftwareRenderer.h"

namespace {

double ms_between(const std::chrono::high_resolution_clock::time_point& start,
                  const std::chrono::high_resolution_clock::time_point& end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

int main() {
    using Clock = std::chrono::high_resolution_clock;

    std::filesystem::create_directories("output");

    vox::VoxelChunk chunk;

    vox::GenerationConfig generation_config;
    generation_config.seed = 424242u;
    generation_config.base_height = 12.0f;
    generation_config.height_variation = 10.5f;
    generation_config.cave_frequency = 0.055f;
    generation_config.cave_threshold = 0.73f;
    generation_config.bedrock_thickness = 2;
    vox::TerrainGenerator generator(generation_config);

    const auto t0 = Clock::now();
    generator.generate_chunk(chunk, vox::Vec3i{0, 0, 0});
    const auto t1 = Clock::now();

    vox::DDGIConfig ddgi_config;
    ddgi_config.cascade_count = 3;
    ddgi_config.rays_per_probe = 56;
    ddgi_config.base_probe_spacing = 2.0f;
    ddgi_config.max_trace_distance = 26.0f;
    ddgi_config.sky_color = vox::Vec3f{0.64f, 0.72f, 0.85f};
    ddgi_config.ambient_floor = vox::Vec3f{0.035f, 0.04f, 0.05f};
    ddgi_config.sun_direction = vox::normalize(vox::Vec3f{0.58f, -1.0f, 0.24f});
    ddgi_config.sun_color = vox::Vec3f{1.14f, 1.05f, 0.94f};
    vox::DDGIVoxelCascade ddgi(ddgi_config);

    ddgi.build(chunk);
    const auto t2 = Clock::now();

    vox::VoxelLightField light_field;
    ddgi.bake_light_field(chunk, light_field);
    const auto t3 = Clock::now();

    vox::BinaryGreedyMesher mesher;
    vox::Mesh mesh = mesher.build(chunk, light_field);
    const auto t4 = Clock::now();

    const std::vector<std::uint8_t> encoded = vox::ChunkCodec::encode(chunk);
    const bool write_ok = vox::ChunkCodec::save_to_file("output/world_chunk.vxm", encoded);

    vox::VoxelChunk decoded;
    const bool decode_ok = vox::ChunkCodec::decode(encoded, decoded);
    const bool roundtrip_ok = decode_ok && chunk.equals(decoded);
    const auto t5 = Clock::now();

    vox::SoftwareRenderer renderer;
    const bool rendered = renderer.render_to_ppm(mesh, "output/frame.ppm", vox::RenderConfig{});
    const auto t6 = Clock::now();

    const vox::GodotMeshBuffers godot_buffers = vox::to_godot_buffers(mesh);

    const double raw_size_bytes = static_cast<double>(vox::kVoxelCount * (sizeof(std::uint16_t) + sizeof(std::uint8_t)));
    const double ratio = raw_size_bytes > 0.0 ? static_cast<double>(encoded.size()) / raw_size_bytes : 0.0;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "TinyVoxel DDGI pipeline\n";
    std::cout << "------------------------\n";
    std::cout << "Chunk size: " << vox::kChunkSize << "^3\n";
    std::cout << "Solid voxels: " << chunk.solid_count() << " / " << vox::kVoxelCount << "\n";

    std::size_t total_probes = 0;
    for (const auto& level : ddgi.levels()) {
        total_probes += static_cast<std::size_t>(level.probe_counts.x) * level.probe_counts.y * level.probe_counts.z;
    }
    std::cout << "DDGI cascades: " << ddgi.levels().size() << " (" << total_probes << " probes total)\n";

    std::cout << "Mesh quads: " << mesh.quad_count << "\n";
    std::cout << "Mesh vertices: " << mesh.vertices.size() << "\n";
    std::cout << "Mesh triangles: " << mesh.indices.size() / 3 << "\n";

    std::cout << "Compressed size: " << encoded.size() << " bytes\n";
    std::cout << "Compression ratio: " << ratio << "x of raw voxel stream\n";

    std::cout << "Godot-ready arrays: "
              << godot_buffers.positions.size() / 3 << " verts, "
              << godot_buffers.indices.size() / 3 << " tris\n";

    std::cout << "\nStage timings (ms):\n";
    std::cout << "  Generate:  " << ms_between(t0, t1) << "\n";
    std::cout << "  DDGI:      " << ms_between(t1, t2) << "\n";
    std::cout << "  Light bake:" << ms_between(t2, t3) << "\n";
    std::cout << "  Meshing:   " << ms_between(t3, t4) << "\n";
    std::cout << "  Codec:     " << ms_between(t4, t5) << "\n";
    std::cout << "  Render:    " << ms_between(t5, t6) << "\n";

    std::cout << "\nArtifacts:\n";
    std::cout << "  output/world_chunk.vxm\n";
    std::cout << "  output/frame.ppm\n";

    if (!write_ok || !roundtrip_ok || !rendered) {
        std::cerr << "\nPipeline finished with errors:";
        if (!write_ok) {
            std::cerr << " [write failed]";
        }
        if (!roundtrip_ok) {
            std::cerr << " [codec roundtrip failed]";
        }
        if (!rendered) {
            std::cerr << " [render failed]";
        }
        std::cerr << "\n";
        return 1;
    }

    return 0;
}
