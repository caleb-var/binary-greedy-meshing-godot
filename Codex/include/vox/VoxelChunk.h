#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "vox/Math.h"

#ifndef VOX_CHUNK_SIZE
#define VOX_CHUNK_SIZE 32
#endif

namespace vox {

constexpr int kChunkSize = VOX_CHUNK_SIZE;
constexpr int kChunkArea = kChunkSize * kChunkSize;
constexpr int kVoxelCount = kChunkArea * kChunkSize;
constexpr int kOccupancyWordCount = (kVoxelCount + 63) / 64;

struct Voxel {
    std::uint16_t material = 0;
    std::uint8_t emission = 0;
};

class VoxelChunk {
public:
    VoxelChunk();

    void clear();

    [[nodiscard]] static constexpr int size() noexcept {
        return kChunkSize;
    }

    [[nodiscard]] static constexpr bool in_bounds(int x, int y, int z) noexcept {
        return x >= 0 && x < kChunkSize && y >= 0 && y < kChunkSize && z >= 0 && z < kChunkSize;
    }

    [[nodiscard]] static constexpr int index_of(int x, int y, int z) noexcept {
        return x + y * kChunkSize + z * kChunkArea;
    }

    void set_voxel(int x, int y, int z, Voxel voxel);
    void set_voxel_linear(int linear_index, Voxel voxel);

    [[nodiscard]] Voxel voxel(int x, int y, int z) const;
    [[nodiscard]] Voxel voxel_linear(int linear_index) const;

    [[nodiscard]] std::uint16_t material(int x, int y, int z) const;
    [[nodiscard]] std::uint8_t emission(int x, int y, int z) const;

    [[nodiscard]] bool is_solid(int x, int y, int z) const;
    [[nodiscard]] bool is_solid_or_oob(int x, int y, int z) const;

    [[nodiscard]] std::size_t solid_count() const noexcept {
        return solid_count_;
    }

    [[nodiscard]] const std::array<std::uint16_t, kVoxelCount>& materials() const noexcept {
        return materials_;
    }

    [[nodiscard]] const std::array<std::uint8_t, kVoxelCount>& emissions() const noexcept {
        return emissions_;
    }

    [[nodiscard]] const std::array<std::uint64_t, kOccupancyWordCount>& occupancy() const noexcept {
        return occupancy_;
    }

    [[nodiscard]] bool equals(const VoxelChunk& rhs) const noexcept;

private:
    void set_occupancy(int linear_index, bool is_solid);

    std::array<std::uint16_t, kVoxelCount> materials_{};
    std::array<std::uint8_t, kVoxelCount> emissions_{};
    std::array<std::uint64_t, kOccupancyWordCount> occupancy_{};
    std::size_t solid_count_ = 0;
};

} // namespace vox
