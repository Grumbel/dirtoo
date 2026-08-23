// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// dirtoo-hilbert-thumb — toy binary-as-Hilbert-curve thumbnailer (CantorDust-ish).
//
// Usage (XDG Thumbnailer1 / tumbler):
//   dirtoo-hilbert-thumb [options] INPUT OUTPUT [SIZE]
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
#include <string_view>
#include <vector>

namespace {

enum class Palette {
  Spectrum, // original: cool→green→red (readable on binaries; green-biased on random)
  Gray,     // luminance only
  Rgb,      // high bits → R/G/B channels (structure without hue bias)
  Hsv,      // hue walks full spectrum at fixed S/V (neutral on uniform random)
  Viridis,  // approx viridis-style ramp (perceptually flatter midtones)
};

void usage(const char* argv0)
{
  std::cerr
      << "Usage: " << argv0 << " [options] <input> <output.png> [size]\n"
      << "  Render a file as a Hilbert-curve binary map (square PNG).\n"
      << "\n"
      << "Options:\n"
      << "  --palette NAME   Color map for byte values (default: spectrum)\n"
      << "  -p NAME          Same as --palette\n"
      << "  -h, --help\n"
      << "\n"
      << "  size             Square edge in pixels (default 128, max 1024)\n"
      << "\n"
      << "Palettes:\n"
      << "  spectrum   Cool→green→red (original). Good for binaries; mid-range\n"
      << "             green dominates high-entropy / compressed data.\n"
      << "  gray       Grayscale intensity of the byte.\n"
      << "  rgb        High bits split across R/G/B (no single-hue bias).\n"
      << "  hsv        Hue = byte/255, fixed saturation/value (full wheel;\n"
      << "             uniform random averages toward muted color, not green).\n"
      << "  viridis    Approx. viridis-style ramp (purple→teal→yellow; flatter).\n"
      << "\n"
      << "XDG thumbnailer: install share/thumbnailers/hilbert-curve.thumbnailer\n";
}

Palette parse_palette(std::string_view s, std::string& err)
{
  std::string lower(s);
  for (char& c : lower) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  if (lower == "spectrum" || lower == "default" || lower == "turbo") {
    return Palette::Spectrum;
  }
  if (lower == "gray" || lower == "grey" || lower == "grayscale") {
    return Palette::Gray;
  }
  if (lower == "rgb" || lower == "bits" || lower == "channels") {
    return Palette::Rgb;
  }
  if (lower == "hsv" || lower == "hue") {
    return Palette::Hsv;
  }
  if (lower == "viridis" || lower == "flat" || lower == "balanced") {
    return Palette::Viridis;
  }
  err = "unknown palette: " + std::string(s)
        + " (use spectrum|gray|rgb|hsv|viridis)";
  return Palette::Spectrum;
}

void spectrum_to_rgb(unsigned char b, unsigned char& r, unsigned char& g, unsigned char& bch)
{
  const float t = static_cast<float>(b) / 255.0f;
  const float r1 = std::clamp(1.5f * t - 0.2f, 0.0f, 1.0f);
  const float g1 = std::clamp(1.5f * (1.0f - std::fabs(t - 0.45f) / 0.45f), 0.0f, 1.0f);
  const float b1 = std::clamp(1.3f * (1.0f - t) - 0.1f, 0.0f, 1.0f);
  const float dim = (b == 0) ? 0.15f : 1.0f;
  r = static_cast<unsigned char>(std::clamp(r1 * dim * 255.0f, 0.0f, 255.0f));
  g = static_cast<unsigned char>(std::clamp(g1 * dim * 255.0f, 0.0f, 255.0f));
  bch = static_cast<unsigned char>(std::clamp(b1 * dim * 255.0f, 0.0f, 255.0f));
}

void gray_to_rgb(unsigned char b, unsigned char& r, unsigned char& g, unsigned char& bch)
{
  const unsigned char v = (b == 0) ? 20 : b;
  r = g = bch = v;
}

void rgb_bits_to_rgb(unsigned char b, unsigned char& r, unsigned char& g, unsigned char& bch)
{
  // High bits → separate channels: random data is multicolored, not green-tinted.
  r = static_cast<unsigned char>(((b >> 5) & 7) * 36);
  g = static_cast<unsigned char>(((b >> 2) & 7) * 36);
  bch = static_cast<unsigned char>((b & 3) * 85);
  if (b == 0) {
    r = g = bch = 20;
  }
}

void hsv_to_rgb(unsigned char b, unsigned char& r, unsigned char& g, unsigned char& bch)
{
  const float h = static_cast<float>(b) / 256.0f;
  const float s = 0.85f;
  const float v = (b == 0) ? 0.12f : 0.90f;
  const float c = v * s;
  const float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
  const float m = v - c;
  float rf = 0, gf = 0, bf = 0;
  const int sector = static_cast<int>(h * 6.0f) % 6;
  switch (sector) {
  case 0:
    rf = c;
    gf = x;
    break;
  case 1:
    rf = x;
    gf = c;
    break;
  case 2:
    gf = c;
    bf = x;
    break;
  case 3:
    gf = x;
    bf = c;
    break;
  case 4:
    rf = x;
    bf = c;
    break;
  default:
    rf = c;
    bf = x;
    break;
  }
  r = static_cast<unsigned char>(std::clamp((rf + m) * 255.0f, 0.0f, 255.0f));
  g = static_cast<unsigned char>(std::clamp((gf + m) * 255.0f, 0.0f, 255.0f));
  bch = static_cast<unsigned char>(std::clamp((bf + m) * 255.0f, 0.0f, 255.0f));
}

void viridis_to_rgb(unsigned char b, unsigned char& r, unsigned char& g, unsigned char& bch)
{
  // Piecewise approx of viridis (purple → teal → yellow); flatter than spectrum.
  const float t = static_cast<float>(b) / 255.0f;
  float rf, gf, bf;
  if (t < 0.25f) {
    const float u = t / 0.25f;
    rf = 0.27f + u * (0.23f - 0.27f);
    gf = 0.00f + u * (0.30f - 0.00f);
    bf = 0.33f + u * (0.55f - 0.33f);
  } else if (t < 0.50f) {
    const float u = (t - 0.25f) / 0.25f;
    rf = 0.23f + u * (0.13f - 0.23f);
    gf = 0.30f + u * (0.53f - 0.30f);
    bf = 0.55f + u * (0.56f - 0.55f);
  } else if (t < 0.75f) {
    const float u = (t - 0.50f) / 0.25f;
    rf = 0.13f + u * (0.37f - 0.13f);
    gf = 0.53f + u * (0.75f - 0.53f);
    bf = 0.56f + u * (0.30f - 0.56f);
  } else {
    const float u = (t - 0.75f) / 0.25f;
    rf = 0.37f + u * (0.99f - 0.37f);
    gf = 0.75f + u * (0.91f - 0.75f);
    bf = 0.30f + u * (0.14f - 0.30f);
  }
  const float dim = (b == 0) ? 0.15f : 1.0f;
  r = static_cast<unsigned char>(std::clamp(rf * dim * 255.0f, 0.0f, 255.0f));
  g = static_cast<unsigned char>(std::clamp(gf * dim * 255.0f, 0.0f, 255.0f));
  bch = static_cast<unsigned char>(std::clamp(bf * dim * 255.0f, 0.0f, 255.0f));
}

void byte_to_rgb(Palette pal, unsigned char b, unsigned char& r, unsigned char& g,
                 unsigned char& bch)
{
  switch (pal) {
  case Palette::Gray:
    gray_to_rgb(b, r, g, bch);
    break;
  case Palette::Rgb:
    rgb_bits_to_rgb(b, r, g, bch);
    break;
  case Palette::Hsv:
    hsv_to_rgb(b, r, g, bch);
    break;
  case Palette::Viridis:
    viridis_to_rgb(b, r, g, bch);
    break;
  case Palette::Spectrum:
  default:
    spectrum_to_rgb(b, r, g, bch);
    break;
  }
}

std::vector<unsigned char> read_file_capped(const std::string& path, std::size_t max_bytes)
{
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("cannot open input: " + path);
  }
  in.seekg(0, std::ios::end);
  const auto sz = static_cast<std::size_t>(std::max<std::streamoff>(0, in.tellg()));
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
  Palette palette = Palette::Spectrum;
  std::vector<std::string> pos;
  pos.reserve(static_cast<std::size_t>(argc));

  for (int i = 1; i < argc; ++i) {
    const std::string_view a{argv[i]};
    if (a == "-h" || a == "--help") {
      usage(argv[0]);
      return 0;
    }
    if (a == "--palette" || a == "-p") {
      if (i + 1 >= argc) {
        std::cerr << "dirtoo-hilbert-thumb: " << a << " requires an argument\n";
        usage(argv[0]);
        return 2;
      }
      std::string err;
      palette = parse_palette(argv[++i], err);
      if (!err.empty()) {
        std::cerr << "dirtoo-hilbert-thumb: " << err << '\n';
        return 2;
      }
      continue;
    }
    if (!a.empty() && a.front() == '-') {
      std::cerr << "dirtoo-hilbert-thumb: unknown option: " << a << '\n';
      usage(argv[0]);
      return 2;
    }
    pos.emplace_back(argv[i]);
  }

  if (pos.size() < 2) {
    usage(argv[0]);
    return 2;
  }

  const std::string& input = pos[0];
  const std::string& output = pos[1];
  int size = 128;
  if (pos.size() >= 3) {
    size = std::atoi(pos[2].c_str());
  }
  if (size < 16) {
    size = 16;
  }
  if (size > 1024) {
    size = 1024;
  }

  try {
    constexpr std::size_t kMaxBytes = 16u * 1024u * 1024u;
    const auto data = read_file_capped(input, kMaxBytes);
    const std::size_t nbytes = data.empty() ? 1 : data.size();

    const std::uint32_t grid = hilbert::next_pow2(static_cast<std::uint32_t>(size));
    const std::uint64_t cells = static_cast<std::uint64_t>(grid) * static_cast<std::uint64_t>(grid);

    std::vector<unsigned char> grid_rgb(static_cast<std::size_t>(cells) * 3);
    for (std::uint64_t d = 0; d < cells; ++d) {
      std::uint32_t x = 0;
      std::uint32_t y = 0;
      hilbert::d2xy(grid, d, x, y);
      const std::size_t start = static_cast<std::size_t>((d * nbytes) / cells);
      std::size_t end = static_cast<std::size_t>(((d + 1) * nbytes) / cells);
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
      unsigned char r = 0, g = 0, b = 0;
      byte_to_rgb(palette, sample, r, g, b);
      const std::size_t pix = (static_cast<std::size_t>(y) * grid + x) * 3;
      grid_rgb[pix + 0] = r;
      grid_rgb[pix + 1] = g;
      grid_rgb[pix + 2] = b;
    }

    std::vector<unsigned char> out_rgb(
        static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 3);
    for (int y = 0; y < size; ++y) {
      const std::uint32_t gy = static_cast<std::uint32_t>(
          (static_cast<std::uint64_t>(y) * grid) / static_cast<std::uint32_t>(size));
      for (int x = 0; x < size; ++x) {
        const std::uint32_t gx = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(x) * grid) / static_cast<std::uint32_t>(size));
        const std::size_t src = (static_cast<std::size_t>(gy) * grid + gx) * 3;
        const std::size_t dst =
            (static_cast<std::size_t>(y) * static_cast<std::size_t>(size)
             + static_cast<std::size_t>(x))
            * 3;
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
