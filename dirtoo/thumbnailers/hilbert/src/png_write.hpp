// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Minimal 8-bit RGB PNG writer (zlib-compressed IDAT).

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <zlib.h>

namespace png_write {

inline void put_u32_be(std::vector<unsigned char>& out, std::uint32_t v)
{
  out.push_back(static_cast<unsigned char>((v >> 24) & 0xff));
  out.push_back(static_cast<unsigned char>((v >> 16) & 0xff));
  out.push_back(static_cast<unsigned char>((v >> 8) & 0xff));
  out.push_back(static_cast<unsigned char>(v & 0xff));
}

inline std::uint32_t crc32_png(const unsigned char* type4, const unsigned char* data, std::size_t len)
{
  std::uint32_t c = crc32(0L, Z_NULL, 0);
  c = crc32(c, type4, 4);
  if (len > 0) {
    c = crc32(c, data, static_cast<uInt>(len));
  }
  return static_cast<std::uint32_t>(c);
}

inline void chunk(std::vector<unsigned char>& out, const char type[4], const unsigned char* data,
                  std::size_t len)
{
  put_u32_be(out, static_cast<std::uint32_t>(len));
  out.push_back(static_cast<unsigned char>(type[0]));
  out.push_back(static_cast<unsigned char>(type[1]));
  out.push_back(static_cast<unsigned char>(type[2]));
  out.push_back(static_cast<unsigned char>(type[3]));
  if (len > 0) {
    out.insert(out.end(), data, data + len);
  }
  const unsigned char t4[4] = {static_cast<unsigned char>(type[0]), static_cast<unsigned char>(type[1]),
                               static_cast<unsigned char>(type[2]), static_cast<unsigned char>(type[3])};
  put_u32_be(out, crc32_png(t4, data, len));
}

/// rgb is size*size*3, row-major, top-left origin, R,G,B per pixel.
inline void write_rgb_file(const std::string& path, int size, const std::vector<unsigned char>& rgb)
{
  if (size <= 0 || static_cast<int>(rgb.size()) < size * size * 3) {
    throw std::runtime_error("png_write: invalid size/rgb");
  }

  // Filter 0 (None) per scanline.
  std::vector<unsigned char> raw;
  raw.reserve(static_cast<std::size_t>(size) * (1 + size * 3));
  for (int y = 0; y < size; ++y) {
    raw.push_back(0);
    const std::size_t row = static_cast<std::size_t>(y) * static_cast<std::size_t>(size) * 3;
    raw.insert(raw.end(), rgb.begin() + static_cast<std::ptrdiff_t>(row),
               rgb.begin() + static_cast<std::ptrdiff_t>(row + static_cast<std::size_t>(size) * 3));
  }

  uLongf bound = compressBound(static_cast<uLong>(raw.size()));
  std::vector<unsigned char> compressed(bound);
  if (compress2(compressed.data(), &bound, raw.data(), static_cast<uLong>(raw.size()), 6) != Z_OK) {
    throw std::runtime_error("png_write: zlib compress failed");
  }
  compressed.resize(bound);

  std::vector<unsigned char> file;
  file.reserve(256 + compressed.size());
  const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  file.insert(file.end(), sig, sig + 8);

  unsigned char ihdr[13];
  ihdr[0] = static_cast<unsigned char>((size >> 24) & 0xff);
  ihdr[1] = static_cast<unsigned char>((size >> 16) & 0xff);
  ihdr[2] = static_cast<unsigned char>((size >> 8) & 0xff);
  ihdr[3] = static_cast<unsigned char>(size & 0xff);
  ihdr[4] = static_cast<unsigned char>((size >> 24) & 0xff);
  ihdr[5] = static_cast<unsigned char>((size >> 16) & 0xff);
  ihdr[6] = static_cast<unsigned char>((size >> 8) & 0xff);
  ihdr[7] = static_cast<unsigned char>(size & 0xff);
  ihdr[8] = 8;  // bit depth
  ihdr[9] = 2;  // RGB
  ihdr[10] = 0; // compression
  ihdr[11] = 0; // filter
  ihdr[12] = 0; // interlace
  chunk(file, "IHDR", ihdr, 13);
  chunk(file, "IDAT", compressed.data(), compressed.size());
  chunk(file, "IEND", nullptr, 0);

  FILE* f = std::fopen(path.c_str(), "wb");
  if (f == nullptr) {
    throw std::runtime_error("png_write: cannot open output");
  }
  const auto n = std::fwrite(file.data(), 1, file.size(), f);
  std::fclose(f);
  if (n != file.size()) {
    throw std::runtime_error("png_write: short write");
  }
}

} // namespace png_write
