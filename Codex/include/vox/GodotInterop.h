#pragma once

#include <cstdint>
#include <vector>

#include "vox/Mesh.h"

namespace vox {

struct GodotMeshBuffers {
    std::vector<float> positions;
    std::vector<float> colors_rgba;
    std::vector<std::uint32_t> indices;
};

[[nodiscard]] GodotMeshBuffers to_godot_buffers(const Mesh& mesh);

} // namespace vox
