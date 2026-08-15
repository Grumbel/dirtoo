// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// dirtoo-hilbert-thumb — toy binary-as-Hilbert-curve thumbnailer (CantorDust-ish).
//
// Usage (XDG Thumbnailer1 / tumbler):
//   dirtoo-hilbert-thumb INPUT OUTPUT [SIZE]
// SIZE is the square edge in pixels (default 128). Grid is the next power of two
// ≥ SIZE; the PNG is SIZE×SIZE (center-crop or scale from the Hilbert grid).

#include "hilbert.hpp"
#include "png_write.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

void usage(const char* argv0)
{
  std::cerr
      << "Usage: " << argv0 << " <input> <output.png> [size]\n"
      << "  Render a file as a Hilbert-curve binary map (square PNG).\n"
      << "  size  square edge in pixels (default 128, max 1024)\n"
      << "\n"
      << "XDG thumbnailer: install share/thumbnailers/hilbert-curve.thumbnailer\n";
}

/// Turbo-inspired spectral palette for byte values (0–255) → RGB.
void byte_to_rgb(unsigned char b, unsigned char& r, unsigned char& g, unsigned char& bch)
{
  // Piecewise map: low bytes cool, mid green/yellow, high red/white — readable
  // structure in executables (headers, zeros, tables, code).
  const float t = static_cast<float>(b) / 255.0f;
  // Simple smooth palette without external LUTs.
  const float r1 = std::clamp(1.5f * t - 0.2f, 0.0f, 1.0f);
  const float g1 = std::clamp(1.5f * (1.0f - std::fabs(t - 0.45f) / 0.45f), 0.0f, 1.0f);
  const float b1 = std::clamp(1.3f * (1.0f - t) - 0.1f, 0.0f, 1.0f);
  // Boost contrast for zero runs (common in binaries).
  const float dim = (b == 0) ? 0.15f : 1.0f;
  r = static_cast<unsigned char>(std::clamp(r1 * dim * 255.0f, 0.0f, 255.0f));
  g = static_cast<unsigned char>(std::clamp(g1 * dim * 255.0f, 0.0f, 255.0f));
  bch = static_cast<unsigned char>(std::clamp(b1 * dim * 255.0f, 0.0f, 255.0f));
}

std::vector<unsigned char> read_file_capped(const std::string& path, std::size_t max_bytes)
{
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("cannot open input: " + path);
  }
  in.seekg(0, std::ios::end);
  const auto sz = static_cast<std::size_t>(in.tellg());
  in.seekg(0, std::ios::beg);
  const std::size_t n = std::min(sz, max_bytes);
  std::vector<unsigned char> data(n);
  if (n > 0) {
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(n));
  }
  return data;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc < 3) {
    usage(argv[0]);
    return 2;
  }
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    }
  }

  const std::string input = argv[1];
  const std::string output = argv[2];
  int size = 128;
  if (argc >= 4) {
    size = std::atoi(argv[3]);
  }
  if (size < 16) {
    size = 16;
  }
  if (size > 1024) {
    size = 1024;
  }

  try {
    // Cap input so huge ISOs do not exhaust memory; still paint full curve.
    constexpr std::size_t kMaxBytes = 16u * 1024u * 1024u;
    const auto data = read_file_capped(input, kMaxBytes);
    const std::size_t nbytes = data.empty() ? 1 : data.size();

    const std::uint32_t grid = hilbert::next_pow2(static_cast<std::uint32_t>(size));
    const std::uint64_t cells = static_cast<std::uint64_t>(grid) * static_cast<std::uint64_t>(grid);

    // Sample along the curve: each cell covers a slice of the file.
    std::vector<unsigned char> grid_rgb(static_cast<std::size_t>(cells) * 3);
    for (std::uint64_t d = 0; d < cells; ++d) {
      std::uint32_t x = 0;
      std::uint32_t y = 0;
      hilbert::d2xy(grid, d, x, y);
      // Map d ∈ [0, cells) → file offset; average a small window for stability.
      const std::size_t start =
          static_cast<std::size_t>((d * nbytes) / cells);
      std::size_t end =
          static_cast<std::size_t>(((d + 1) * nbytes) / cells);
      if (end <= start) {
        end = start + 1;
      }
      if (end > nbytes) {
        end = nbytes;
      }
      unsigned acc = 0;
      unsigned cnt = 0;
      for (std::size_t i = start; i < end; ++i) {
        acc += data.empty() ? 0 : data[i];
        ++cnt;
      }
      const unsigned char sample =
          static_cast<unsigned char>(cnt ? (acc / cnt) : 0);
      unsigned char r, g, b;
      byte_to_rgb(sample, r, g, b);
      const std::size_t pix = (static_cast<std::size_t>(y) * grid + x) * 3;
      grid_rgb[pix + 0] = r;
      grid_rgb[pix + 1] = g;
      grid_rgb[pix + 2] = b;
    }

    // Nearest-neighbor scale grid → size×size output.
    std::vector<unsigned char> out_rgb(static_cast<std::size_t>(size) * static_cast<std::size_t>(size)
                                       * 3);
    for (int y = 0; y < size; ++y) {
      const std::uint32_t gy =
          static_cast<std::uint32_t>((static_cast<std::uint64_t>(y) * grid) / static_cast<std::uint32_t>(size));
      for (int x = 0; x < size; ++x) {
        const std::uint32_t gx =
            static_cast<std::uint32_t>((static_cast<std::uint64_t>(x) * grid) / static_cast<std::uint32_t>(size));
        const std::size_t src = (static_cast<std::size_t>(gy) * grid + gx) * 3;
        const std::size_t dst =
            (static_cast<std::size_t>(y) * static_cast<std::size_t>(size) + static_cast<std::size_t>(x)) * 3;
        out_rgb[dst + 0] = grid_rgb[src + 0];
        out_rgb[dst + 1] = grid_rgb[src + 1];
        out_rgb[dst + 2] = grid_rgb[src + 2];
      }
    }

    png_write::write_rgb_file(output, size, out_rgb);
  } catch (const std::exception& ex) {
    std::cerr << "dirtoo-hilbert-thumb: " << ex.what() << '\n';
    return 1;
  }
  return 0;
}
