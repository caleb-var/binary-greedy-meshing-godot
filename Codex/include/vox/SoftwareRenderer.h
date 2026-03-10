#pragma once

#include <filesystem>

#include "vox/Mesh.h"

namespace vox {

struct RenderConfig {
    int width = 1280;
    int height = 720;
    float yaw_degrees = 45.0f;
    float pitch_degrees = -35.0f;
    float zoom = 3.6f;
    float camera_distance = 48.0f;
    Vec3f target{16.0f, 10.0f, 16.0f};
    Vec3f background{0.03f, 0.05f, 0.08f};
};

class SoftwareRenderer {
public:
    [[nodiscard]] bool render_to_ppm(
        const Mesh& mesh,
        const std::filesystem::path& output_path,
        const RenderConfig& config = {}
    ) const;
};

} // namespace vox
