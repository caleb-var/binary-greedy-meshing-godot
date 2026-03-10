#include "vox/ChunkCodec.h"

#include <array>
#include <cstddef>
#include <fstream>

namespace vox {
namespace {

constexpr std::array<std::uint8_t, 4> kMagic = {'V', 'X', 'M', 'P'};
constexpr std::uint16_t kVersion = 1;

void write_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
}

void write_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
}

std::uint16_t read_u16(std::span<const std::uint8_t> data, std::size_t& offset) {
    const std::uint16_t lo = data[offset++];
    const std::uint16_t hi = data[offset++];
    return static_cast<std::uint16_t>(lo | (hi << 8));
}

std::uint32_t read_u32(std::span<const std::uint8_t> data, std::size_t& offset) {
    const std::uint32_t b0 = data[offset++];
    const std::uint32_t b1 = data[offset++];
    const std::uint32_t b2 = data[offset++];
    const std::uint32_t b3 = data[offset++];
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

void write_varint(std::vector<std::uint8_t>& out, std::uint32_t value) {
    while (value >= 0x80u) {
        out.push_back(static_cast<std::uint8_t>((value & 0x7fu) | 0x80u));
        value >>= 7;
    }
    out.push_back(static_cast<std::uint8_t>(value));
}

bool read_varint(std::span<const std::uint8_t> data, std::size_t& offset, std::uint32_t& value) {
    value = 0;
    int shift = 0;

    while (offset < data.size() && shift <= 28) {
        const std::uint8_t byte = data[offset++];
        value |= static_cast<std::uint32_t>(byte & 0x7fu) << shift;
        if ((byte & 0x80u) == 0u) {
            return true;
        }
        shift += 7;
    }

    return false;
}

} // namespace

std::vector<std::uint8_t> ChunkCodec::encode(const VoxelChunk& chunk) {
    std::vector<std::uint8_t> output;
    output.reserve(4096);

    output.insert(output.end(), kMagic.begin(), kMagic.end());
    write_u16(output, kVersion);
    output.push_back(static_cast<std::uint8_t>(kChunkSize));
    output.push_back(0u);
    write_u32(output, static_cast<std::uint32_t>(kVoxelCount));

    int index = 0;
    while (index < kVoxelCount) {
        const Voxel first = chunk.voxel_linear(index);
        std::uint32_t run_length = 1;

        while (index + static_cast<int>(run_length) < kVoxelCount) {
            const Voxel next = chunk.voxel_linear(index + static_cast<int>(run_length));
            if (next.material != first.material || next.emission != first.emission) {
                break;
            }
            ++run_length;
        }

        write_varint(output, run_length);
        write_u16(output, first.material);
        output.push_back(first.emission);

        index += static_cast<int>(run_length);
    }

    return output;
}

bool ChunkCodec::decode(std::span<const std::uint8_t> data, VoxelChunk& out_chunk) {
    if (data.size() < 12) {
        return false;
    }

    std::size_t offset = 0;
    for (std::uint8_t expected : kMagic) {
        if (data[offset++] != expected) {
            return false;
        }
    }

    const std::uint16_t version = read_u16(data, offset);
    if (version != kVersion) {
        return false;
    }

    const std::uint8_t chunk_size = data[offset++];
    offset++; // reserved
    if (chunk_size != static_cast<std::uint8_t>(kChunkSize)) {
        return false;
    }

    const std::uint32_t expected_voxels = read_u32(data, offset);
    if (expected_voxels != static_cast<std::uint32_t>(kVoxelCount)) {
        return false;
    }

    out_chunk.clear();
    int write_index = 0;

    while (write_index < kVoxelCount && offset < data.size()) {
        std::uint32_t run_length = 0;
        if (!read_varint(data, offset, run_length) || run_length == 0) {
            return false;
        }

        if (offset + 3 > data.size()) {
            return false;
        }

        const std::uint16_t material = read_u16(data, offset);
        const std::uint8_t emission = data[offset++];

        for (std::uint32_t i = 0; i < run_length; ++i) {
            if (write_index >= kVoxelCount) {
                return false;
            }
            out_chunk.set_voxel_linear(write_index++, Voxel{material, emission});
        }
    }

    return write_index == kVoxelCount;
}

bool ChunkCodec::save_to_file(const std::filesystem::path& path, std::span<const std::uint8_t> data) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return file.good();
}

std::vector<std::uint8_t> ChunkCodec::load_from_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }

    file.seekg(0, std::ios::end);
    const std::streamoff length = file.tellg();
    file.seekg(0, std::ios::beg);

    if (length <= 0) {
        return {};
    }

    std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
    file.read(reinterpret_cast<char*>(data.data()), length);
    if (!file.good() && !file.eof()) {
        return {};
    }

    return data;
}

} // namespace vox

