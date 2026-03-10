#include "vox/SoftwareRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <vector>

namespace vox {
namespace {

struct ScreenVertex {
    float x = 0.0f;
    float y = 0.0f;
    float depth = 0.0f;
    Vec3f color{};
    bool valid = false;
};

float edge_function(float ax, float ay, float bx, float by, float cx, float cy) {
    return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
}

bool project_vertex(const Vertex& vertex, const RenderConfig& cfg, ScreenVertex& out) {
    const float yaw = cfg.yaw_degrees * (kPi / 180.0f);
    const float pitch = cfg.pitch_degrees * (kPi / 180.0f);

    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);

    Vec3f p = vertex.position - cfg.target;

    const float x1 = cy * p.x + sy * p.z;
    const float z1 = -sy * p.x + cy * p.z;

    const float y2 = cp * p.y - sp * z1;
    const float z2 = sp * p.y + cp * z1 + cfg.camera_distance;

    if (z2 <= 0.001f) {
        return false;
    }

    const float scale = (cfg.zoom * static_cast<float>(cfg.width)) / z2;

    out.x = static_cast<float>(cfg.width) * 0.5f + x1 * scale;
    out.y = static_cast<float>(cfg.height) * 0.5f - y2 * scale;
    out.depth = z2;
    out.color = unpack_rgba8(vertex.color_rgba8);
    out.valid = true;
    return true;
}

} // namespace

bool SoftwareRenderer::render_to_ppm(const Mesh& mesh, const std::filesystem::path& output_path, const RenderConfig& config) const {
    if (config.width <= 0 || config.height <= 0) {
        return false;
    }

    std::vector<std::uint8_t> framebuffer(static_cast<std::size_t>(config.width * config.height * 3));
    std::vector<float> depthbuffer(static_cast<std::size_t>(config.width * config.height), std::numeric_limits<float>::infinity());

    const std::uint8_t bg_r = to_u8(srgb_from_linear(config.background.x));
    const std::uint8_t bg_g = to_u8(srgb_from_linear(config.background.y));
    const std::uint8_t bg_b = to_u8(srgb_from_linear(config.background.z));

    for (std::size_t i = 0; i < framebuffer.size(); i += 3) {
        framebuffer[i + 0] = bg_r;
        framebuffer[i + 1] = bg_g;
        framebuffer[i + 2] = bg_b;
    }

    std::vector<ScreenVertex> projected(mesh.vertices.size());
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        project_vertex(mesh.vertices[i], config, projected[i]);
    }

    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const std::uint32_t ia = mesh.indices[i + 0];
        const std::uint32_t ib = mesh.indices[i + 1];
        const std::uint32_t ic = mesh.indices[i + 2];

        if (ia >= projected.size() || ib >= projected.size() || ic >= projected.size()) {
            continue;
        }

        const ScreenVertex& a = projected[ia];
        const ScreenVertex& b = projected[ib];
        const ScreenVertex& c = projected[ic];
        if (!a.valid || !b.valid || !c.valid) {
            continue;
        }

        const float area = edge_function(a.x, a.y, b.x, b.y, c.x, c.y);
        if (std::fabs(area) < 1e-6f) {
            continue;
        }

        const float min_x = std::max(0.0f, std::floor(std::min({a.x, b.x, c.x})));
        const float max_x = std::min(static_cast<float>(config.width - 1), std::ceil(std::max({a.x, b.x, c.x})));
        const float min_y = std::max(0.0f, std::floor(std::min({a.y, b.y, c.y})));
        const float max_y = std::min(static_cast<float>(config.height - 1), std::ceil(std::max({a.y, b.y, c.y})));

        for (int y = static_cast<int>(min_y); y <= static_cast<int>(max_y); ++y) {
            for (int x = static_cast<int>(min_x); x <= static_cast<int>(max_x); ++x) {
                const float px = static_cast<float>(x) + 0.5f;
                const float py = static_cast<float>(y) + 0.5f;

                const float w0 = edge_function(b.x, b.y, c.x, c.y, px, py);
                const float w1 = edge_function(c.x, c.y, a.x, a.y, px, py);
                const float w2 = edge_function(a.x, a.y, b.x, b.y, px, py);

                if ((w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) && (w0 > 0.0f || w1 > 0.0f || w2 > 0.0f)) {
                    continue;
                }

                const float inv_area = 1.0f / area;
                const float lambda0 = w0 * inv_area;
                const float lambda1 = w1 * inv_area;
                const float lambda2 = w2 * inv_area;

                const float depth = lambda0 * a.depth + lambda1 * b.depth + lambda2 * c.depth;
                const std::size_t pixel_index = static_cast<std::size_t>(y * config.width + x);
                if (depth >= depthbuffer[pixel_index]) {
                    continue;
                }

                depthbuffer[pixel_index] = depth;

                const Vec3f linear =
                    a.color * lambda0 +
                    b.color * lambda1 +
                    c.color * lambda2;

                framebuffer[pixel_index * 3 + 0] = to_u8(srgb_from_linear(linear.x));
                framebuffer[pixel_index * 3 + 1] = to_u8(srgb_from_linear(linear.y));
                framebuffer[pixel_index * 3 + 2] = to_u8(srgb_from_linear(linear.z));
            }
        }
    }

    std::error_code ec;
    std::filesystem::create_directories(output_path.parent_path(), ec);

    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        return false;
    }

    output << "P6\n" << config.width << " " << config.height << "\n255\n";
    output.write(reinterpret_cast<const char*>(framebuffer.data()), static_cast<std::streamsize>(framebuffer.size()));
    return output.good();
}

} // namespace vox
