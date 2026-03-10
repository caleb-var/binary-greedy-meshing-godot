#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#define BM_IMPLEMENTATION
#include "../src/mesher.h"
#include "../src/data/rle.h"

struct ChunkTableEntry {
  uint32_t key;
  uint32_t rleDataBegin;
  uint32_t rleDataSize;
};

struct LevelData {
  uint8_t size = 0;
  std::vector<uint8_t> buffer;
  std::vector<ChunkTableEntry> table;
};

LevelData loadLevelFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("failed to open level file: " + path);
  }

  file.seekg(0, std::ios::end);
  const auto fileSize = static_cast<size_t>(file.tellg());
  file.seekg(0, std::ios::beg);

  LevelData level;
  level.buffer.resize(fileSize);
  file.read(reinterpret_cast<char*>(level.buffer.data()), static_cast<std::streamsize>(fileSize));

  level.size = level.buffer[0];
  const uint32_t tableLength = static_cast<uint32_t>(level.size) * static_cast<uint32_t>(level.size);
  level.table.resize(tableLength);

  const size_t tableOffset = 1;
  const size_t tableBytes = tableLength * sizeof(ChunkTableEntry);
  std::memcpy(level.table.data(), level.buffer.data() + tableOffset, tableBytes);

  return level;
}

struct Stats {
  double avg = 0;
  double p50 = 0;
  double p95 = 0;
  double min = 0;
  double max = 0;
};

Stats computeStats(const std::vector<double>& samples) {
  std::vector<double> sorted = samples;
  std::sort(sorted.begin(), sorted.end());

  Stats s;
  s.avg = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
  s.p50 = sorted[sorted.size() / 2];
  s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
  s.min = sorted.front();
  s.max = sorted.back();
  return s;
}

int main() {
  const std::string levelPath = "levels/demo_terrain_96";
  const int measuredPasses = 3;

  const LevelData level = loadLevelFile(levelPath);

  std::vector<uint8_t> voxels(CS_P3, 0);
  std::vector<uint64_t> opaqueMask(CS_P2, 0);
  std::vector<uint64_t> faceMasks(CS_2 * 6, 0);
  std::vector<uint8_t> forwardMerged(CS_2, 0);
  std::vector<uint8_t> rightMerged(CS, 0);
  std::vector<uint64_t> vertices(20000, 0);

  MeshData md;
  md.opaqueMask = opaqueMask.data();
  md.faceMasks = faceMasks.data();
  md.forwardMerged = forwardMerged.data();
  md.rightMerged = rightMerged.data();
  md.vertices = &vertices;
  md.maxVertices = static_cast<int>(vertices.size());

  // Warmup pass.
  for (const auto& entry : level.table) {
    std::memset(voxels.data(), 0, CS_P3);
    std::memset(md.opaqueMask, 0, CS_P2 * sizeof(uint64_t));
    rle::decompressToVoxelsAndOpaqueMask(const_cast<uint8_t*>(level.buffer.data()) + entry.rleDataBegin, entry.rleDataSize, voxels.data(), md.opaqueMask);
    mesh(voxels.data(), md);
  }

  std::vector<double> decompressUs;
  std::vector<double> meshUs;
  std::vector<double> passWallMs;
  decompressUs.reserve(level.table.size() * measuredPasses);
  meshUs.reserve(level.table.size() * measuredPasses);

  long long totalVertices = 0;

  for (int pass = 0; pass < measuredPasses; ++pass) {
    const auto passStart = std::chrono::steady_clock::now();

    for (const auto& entry : level.table) {
      std::memset(voxels.data(), 0, CS_P3);
      std::memset(md.opaqueMask, 0, CS_P2 * sizeof(uint64_t));

      const auto d0 = std::chrono::steady_clock::now();
      rle::decompressToVoxelsAndOpaqueMask(const_cast<uint8_t*>(level.buffer.data()) + entry.rleDataBegin, entry.rleDataSize, voxels.data(), md.opaqueMask);
      const auto d1 = std::chrono::steady_clock::now();
      const std::chrono::duration<double, std::micro> dUs = d1 - d0;
      decompressUs.push_back(dUs.count());

      const auto m0 = std::chrono::steady_clock::now();
      mesh(voxels.data(), md);
      const auto m1 = std::chrono::steady_clock::now();
      const std::chrono::duration<double, std::micro> mUs = m1 - m0;
      meshUs.push_back(mUs.count());

      totalVertices += md.vertexCount;
    }

    const auto passEnd = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> passMs = passEnd - passStart;
    passWallMs.push_back(passMs.count());
  }

  const Stats dStats = computeStats(decompressUs);
  const Stats mStats = computeStats(meshUs);

  const double avgPassMs = std::accumulate(passWallMs.begin(), passWallMs.end(), 0.0) / passWallMs.size();
  const double chunksPerPass = static_cast<double>(level.table.size());

  std::cout << "Level benchmark: " << levelPath << "\n";
  std::cout << "World size: " << static_cast<int>(level.size) << "x" << static_cast<int>(level.size)
            << " chunks (" << level.table.size() << " chunks total)\n";
  std::cout << "Measured passes: " << measuredPasses << "\n\n";

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "Decompression per chunk (us): avg=" << dStats.avg
            << " p50=" << dStats.p50
            << " p95=" << dStats.p95
            << " min=" << dStats.min
            << " max=" << dStats.max << "\n";
  std::cout << "Meshing per chunk (us):       avg=" << mStats.avg
            << " p50=" << mStats.p50
            << " p95=" << mStats.p95
            << " min=" << mStats.min
            << " max=" << mStats.max << "\n";
  std::cout << "Average pass wall time (ms):  " << avgPassMs << "\n";
  std::cout << "Average wall time / chunk (us): " << (avgPassMs * 1000.0 / chunksPerPass) << "\n";
  std::cout << "Average vertices / chunk: "
            << (static_cast<double>(totalVertices) / (chunksPerPass * measuredPasses)) << "\n";

  return 0;
}
