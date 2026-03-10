#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_BIN="$ROOT_DIR/benchmarks/level96_benchmark"

g++ -O3 -march=native -std=c++17 "$ROOT_DIR/benchmarks/level96_benchmark.cpp" -o "$OUT_BIN"
"$OUT_BIN"
