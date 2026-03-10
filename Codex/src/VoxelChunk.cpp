#include "vox/VoxelChunk.h"

#include <algorithm>
#include <cassert>

namespace vox {

VoxelChunk::VoxelChunk() {
    clear();
}

void VoxelChunk::clear() {
    std::fill(materials_.begin(), materials_.end(), 0);
    std::fill(emissions_.begin(), emissions_.end(), 0);
    std::fill(occupancy_.begin(), occupancy_.end(), 0ull);
    solid_count_ = 0;
}

void VoxelChunk::set_occupancy(int linear_index, bool is_solid_now) {
    const int word_index = linear_index >> 6;
    const std::uint64_t bit = 1ull << (linear_index & 63);
    const bool was_solid = (occupancy_[word_index] & bit) != 0ull;
    if (was_solid == is_solid_now) {
        return;
    }

    if (is_solid_now) {
        occupancy_[word_index] |= bit;
        ++solid_count_;
    } else {
        occupancy_[word_index] &= ~bit;
        --solid_count_;
    }
}

void VoxelChunk::set_voxel(int x, int y, int z, Voxel voxel_value) {
    assert(in_bounds(x, y, z));
    set_voxel_linear(index_of(x, y, z), voxel_value);
}

void VoxelChunk::set_voxel_linear(int linear_index, Voxel voxel_value) {
    assert(linear_index >= 0 && linear_index < kVoxelCount);
    materials_[linear_index] = voxel_value.material;
    emissions_[linear_index] = voxel_value.emission;
    set_occupancy(linear_index, voxel_value.material != 0);
}

Voxel VoxelChunk::voxel(int x, int y, int z) const {
    assert(in_bounds(x, y, z));
    return voxel_linear(index_of(x, y, z));
}

Voxel VoxelChunk::voxel_linear(int linear_index) const {
    assert(linear_index >= 0 && linear_index < kVoxelCount);
    return Voxel{materials_[linear_index], emissions_[linear_index]};
}

std::uint16_t VoxelChunk::material(int x, int y, int z) const {
    assert(in_bounds(x, y, z));
    return materials_[index_of(x, y, z)];
}

std::uint8_t VoxelChunk::emission(int x, int y, int z) const {
    assert(in_bounds(x, y, z));
    return emissions_[index_of(x, y, z)];
}

bool VoxelChunk::is_solid(int x, int y, int z) const {
    if (!in_bounds(x, y, z)) {
        return false;
    }
    const int linear_index = index_of(x, y, z);
    return (occupancy_[linear_index >> 6] >> (linear_index & 63)) & 1ull;
}

bool VoxelChunk::is_solid_or_oob(int x, int y, int z) const {
    return !in_bounds(x, y, z) || is_solid(x, y, z);
}

bool VoxelChunk::equals(const VoxelChunk& rhs) const noexcept {
    return materials_ == rhs.materials_ && emissions_ == rhs.emissions_;
}

} // namespace vox
