#include "vox/GodotInterop.h"

namespace vox {

GodotMeshBuffers to_godot_buffers(const Mesh& mesh) {
    GodotMeshBuffers buffers;
    buffers.positions.reserve(mesh.vertices.size() * 3);
    buffers.colors_rgba.reserve(mesh.vertices.size() * 4);
    buffers.indices = mesh.indices;

    for (const Vertex& vertex : mesh.vertices) {
        buffers.positions.push_back(vertex.position.x);
        buffers.positions.push_back(vertex.position.y);
        buffers.positions.push_back(vertex.position.z);

        const Vec3f linear = unpack_rgba8(vertex.color_rgba8);
        buffers.colors_rgba.push_back(linear.x);
        buffers.colors_rgba.push_back(linear.y);
        buffers.colors_rgba.push_back(linear.z);
        buffers.colors_rgba.push_back(1.0f);
    }

    return buffers;
}

} // namespace vox
