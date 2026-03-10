#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#define BM_IMPLEMENTATION
#include "../src/mesher.h"

namespace {

inline int voxelIndex(int z, int x, int y) {
  return z + x * CS_P + y * CS_P2;
}

void buildOpaqueMask(const std::vector<uint8_t>& voxels, uint64_t* opaqueMask) {
  std::fill_n(opaqueMask, CS_P2, 0ull);

  for (int x = 0; x < CS_P; ++x) {
    for (int y = 0; y < CS_P; ++y) {
      uint64_t bits = 0;
      for (int z = 0; z < CS_P; ++z) {
        if (voxels[voxelIndex(z, x, y)] != 0) {
          bits |= (1ull << z);
        }
      }
      opaqueMask[x * CS_P + y] = bits;
    }
  }
}

struct BenchResult {
  std::string name;
  double avgUs;
  double p50Us;
  double p95Us;
  double minUs;
  double maxUs;
  double avgQuads;
};

BenchResult runCase(const std::string& name, const std::vector<uint8_t>& voxels, int iterations) {
  std::vector<uint64_t> faceMasks(CS_2 * 6, 0);
  std::vector<uint64_t> opaqueMask(CS_P2, 0);
  std::vector<uint8_t> forwardMerged(CS_2, 0);
  std::vector<uint8_t> rightMerged(CS, 0);

  MeshData md;
  md.faceMasks = faceMasks.data();
  md.opaqueMask = opaqueMask.data();
  md.forwardMerged = forwardMerged.data();
  md.rightMerged = rightMerged.data();
  md.vertices = new std::vector<uint64_t>(4096, 0);
  md.maxVertices = static_cast<int>(md.vertices->size());

  buildOpaqueMask(voxels, md.opaqueMask);

  for (int i = 0; i < 200; ++i) {
    mesh(voxels.data(), md);
  }

  std::vector<double> samplesUs;
  samplesUs.reserve(iterations);
  std::vector<int> quadCounts;
  quadCounts.reserve(iterations);

  for (int i = 0; i < iterations; ++i) {
    const auto start = std::chrono::steady_clock::now();
    mesh(voxels.data(), md);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double, std::micro> durationUs = end - start;
    samplesUs.push_back(durationUs.count());
    quadCounts.push_back(md.vertexCount);
  }

  delete md.vertices;

  std::vector<double> sorted = samplesUs;
  std::sort(sorted.begin(), sorted.end());

  const double avg = std::accumulate(samplesUs.begin(), samplesUs.end(), 0.0) / samplesUs.size();
  const double avgQuads = std::accumulate(quadCounts.begin(), quadCounts.end(), 0.0) / quadCounts.size();

  return BenchResult {
    name,
    avg,
    sorted[sorted.size() / 2],
    sorted[static_cast<size_t>(sorted.size() * 0.95)],
    sorted.front(),
    sorted.back(),
    avgQuads,
  };
}

std::vector<uint8_t> makeEmpty() {
  return std::vector<uint8_t>(CS_P3, 0);
}

std::vector<uint8_t> makeSolidChunk() {
  std::vector<uint8_t> voxels(CS_P3, 0);
  for (int x = 1; x <= CS; ++x) {
    for (int y = 1; y <= CS; ++y) {
      for (int z = 1; z <= CS; ++z) {
        voxels[voxelIndex(z, x, y)] = 1;
      }
    }
  }
  return voxels;
}

std::vector<uint8_t> makeRandomNoise(uint32_t seed, double fillRate) {
  std::mt19937 rng(seed);
  std::bernoulli_distribution dist(fillRate);

  std::vector<uint8_t> voxels(CS_P3, 0);
  for (int x = 1; x <= CS; ++x) {
    for (int y = 1; y <= CS; ++y) {
      for (int z = 1; z <= CS; ++z) {
        voxels[voxelIndex(z, x, y)] = dist(rng) ? static_cast<uint8_t>(1 + (z % 3)) : 0;
      }
    }
  }
  return voxels;
}

std::vector<uint8_t> makeHeightTerrain() {
  std::vector<uint8_t> voxels(CS_P3, 0);
  for (int x = 1; x <= CS; ++x) {
    for (int y = 1; y <= CS; ++y) {
      const double fx = static_cast<double>(x - 1) / CS;
      const double fy = static_cast<double>(y - 1) / CS;
      const double h = 16.0 + 22.0 * std::sin(fx * 8.5) + 14.0 * std::cos(fy * 6.0);
      const int height = std::clamp(static_cast<int>(std::round(h)), 1, CS);

      for (int z = 1; z <= height; ++z) {
        voxels[voxelIndex(z, x, y)] = static_cast<uint8_t>(1 + (z > height - 4));
      }
    }
  }
  return voxels;
}

} // namespace

int main() {
  const int iterations = 5000;

  const std::vector<BenchResult> results {
    runCase("empty", makeEmpty(), iterations),
    runCase("solid(62^3)", makeSolidChunk(), iterations),
    runCase("terrain(heightmap)", makeHeightTerrain(), iterations),
    runCase("noise(50% fill)", makeRandomNoise(42, 0.5), iterations),
  };

  std::cout << "Binary Greedy Mesher Benchmark (mesh() only)\n";
  std::cout << "Chunk core size: " << CS << "^3, padded input: " << CS_P << "^3\n";
  std::cout << "Iterations per case: " << iterations << "\n\n";

  std::cout << std::left
            << std::setw(20) << "case"
            << std::right
            << std::setw(12) << "avg(us)"
            << std::setw(12) << "p50(us)"
            << std::setw(12) << "p95(us)"
            << std::setw(12) << "min(us)"
            << std::setw(12) << "max(us)"
            << std::setw(12) << "avgQuads"
            << "\n";

  for (const auto& r : results) {
    std::cout << std::left << std::setw(20) << r.name
              << std::right << std::fixed << std::setprecision(2)
              << std::setw(12) << r.avgUs
              << std::setw(12) << r.p50Us
              << std::setw(12) << r.p95Us
              << std::setw(12) << r.minUs
              << std::setw(12) << r.maxUs
              << std::setw(12) << r.avgQuads
              << "\n";
  }

  return 0;
}
