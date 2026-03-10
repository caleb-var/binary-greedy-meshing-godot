#pragma once

#include <filesystem>
#include <span>
#include <vector>

#include "vox/VoxelChunk.h"

namespace vox {

class ChunkCodec {
public:
    [[nodiscard]] static std::vector<std::uint8_t> encode(const VoxelChunk& chunk);
    [[nodiscard]] static bool decode(std::span<const std::uint8_t> data, VoxelChunk& out_chunk);

    [[nodiscard]] static bool save_to_file(const std::filesystem::path& path, std::span<const std::uint8_t> data);
    [[nodiscard]] static std::vector<std::uint8_t> load_from_file(const std::filesystem::path& path);
};

} // namespace vox
