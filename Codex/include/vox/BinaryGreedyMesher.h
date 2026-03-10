#pragma once

#include "vox/Light.h"
#include "vox/Mesh.h"
#include "vox/VoxelChunk.h"

namespace vox {

class BinaryGreedyMesher {
public:
    [[nodiscard]] Mesh build(const VoxelChunk& chunk, const VoxelLightField& light_field) const;

private:
    void emit_quad(
        Mesh& mesh,
        int axis,
        int sign,
        int slice,
        int u0,
        int v0,
        int width,
        int height,
        std::uint32_t face_key
    ) const;
};

} // namespace vox
