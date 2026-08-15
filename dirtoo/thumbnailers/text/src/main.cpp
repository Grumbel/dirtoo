// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// dirtoo-text-thumb — layout-preview thumbnail for text files.
// White background, black 5×7 glyphs. Up to three 80-character columns:
// start / middle / end of the file (fewer columns when the document is short).

#include "font5x7.hpp"
#include "png_write.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int kColChars = 80;
constexpr std::size_t kMaxBytes = 512u * 1024u;

void usage(const char* argv0)
{
  std::cerr << "Usage: " << argv0 << " <input> <output.png> [size]\n"
            << "  Text layout thumbnail (1–3 columns of 80 chars: start/mid/end).\n";
}

std::string read_text_capped(const std::string& path)
{
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("cannot open input: " + path);
  }
  in.seekg(0, std::ios::end);
  const auto sz = static_cast<std::size_t>(std::max<std::streamoff>(0, in.tellg()));
  in.seekg(0, std::ios::beg);
  const std::size_t n = std::min(sz, kMaxBytes);
  std::string s(n, '\0');
  if (n > 0) {
    in.read(s.data(), static_cast<std::streamsize>(n));
  }
  // Normalize newlines; drop CR; map non-printables (except tab/newline) to space.
  std::string out;
  out.reserve(s.size());
  for (unsigned char ch : s) {
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n' || ch == '\t' || (ch >= 32 && ch < 127)) {
      out.push_back(static_cast<char>(ch == '\t' ? ' ' : ch));
    } else if (ch >= 128) {
      out.push_back('.'); // high bytes as dots so UTF-8 still shows density
    } else {
      out.push_back(' ');
    }
  }
  return out;
}

/// Wrap a linear text window into lines of at most kColChars (hard crop).
std::vector<std::string> wrap_window(const std::string& text, std::size_t begin, std::size_t end,
                                     int max_lines)
{
  std::vector<std::string> lines;
  if (begin >= text.size()) {
    return lines;
  }
  end = std::min(end, text.size());
  std::size_t i = begin;
  while (i < end && static_cast<int>(lines.size()) < max_lines) {
    // Prefer newline breaks within the window.
    std::size_t line_end = i;
    std::size_t limit = std::min(i + static_cast<std::size_t>(kColChars), end);
    std::size_t nl = text.find('\n', i);
    if (nl != std::string::npos && nl < limit) {
      line_end = nl;
      lines.push_back(text.substr(i, line_end - i));
      i = nl + 1;
      continue;
    }
    line_end = limit;
    lines.push_back(text.substr(i, line_end - i));
    i = line_end;
    if (i < end && text[i] == '\n') {
      ++i;
    }
  }
  return lines;
}

void plot_char(std::vector<unsigned char>& rgb, int size, int px, int py, unsigned char ch)
{
  const std::uint8_t* g = font5x7::glyph(ch);
  for (int row = 0; row < font5x7::kH; ++row) {
    const std::uint8_t bits = g[row];
    for (int col = 0; col < font5x7::kW; ++col) {
      if ((bits & (1u << (font5x7::kW - 1 - col))) == 0) {
        continue;
      }
      const int x = px + col;
      const int y = py + row;
      if (x < 0 || y < 0 || x >= size || y >= size) {
        continue;
      }
      const std::size_t o = (static_cast<std::size_t>(y) * static_cast<std::size_t>(size)
                             + static_cast<std::size_t>(x))
                            * 3;
      rgb[o + 0] = 0;
      rgb[o + 1] = 0;
      rgb[o + 2] = 0;
    }
  }
}

void draw_column(std::vector<unsigned char>& rgb, int size, int origin_x, int origin_y,
                 const std::vector<std::string>& lines, int max_lines)
{
  for (int li = 0; li < static_cast<int>(lines.size()) && li < max_lines; ++li) {
    const std::string& line = lines[static_cast<std::size_t>(li)];
    const int y = origin_y + li * font5x7::kCellH;
    const int n = std::min(static_cast<int>(line.size()), kColChars);
    for (int ci = 0; ci < n; ++ci) {
      const int x = origin_x + ci * font5x7::kCellW;
      plot_char(rgb, size, x, y, static_cast<unsigned char>(line[static_cast<std::size_t>(ci)]));
    }
  }
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
  size = std::clamp(size, 32, 1024);

  try {
    const std::string text = read_text_capped(input);

    // How many lines fit vertically with 1px padding.
    const int pad = 2;
    const int usable_h = std::max(1, size - 2 * pad);
    const int max_lines = std::max(1, usable_h / font5x7::kCellH);

    // Column width in pixels for 80 glyph cells.
    const int col_px = kColChars * font5x7::kCellW;
    const int gap_px = 3;

    // Characters that one full column can show.
    const std::size_t per_col = static_cast<std::size_t>(max_lines) * static_cast<std::size_t>(kColChars);
    const std::size_t len = text.size();

    int ncols = 1;
    if (len > per_col * 2) {
      ncols = 3;
    } else if (len > per_col) {
      ncols = 2;
    }

    // Scale: fit ncols columns into the square.
    const int need_w = ncols * col_px + (ncols - 1) * gap_px + 2 * pad;
    const int need_h = max_lines * font5x7::kCellH + 2 * pad;
    const float scale = std::min(1.0f, std::min(static_cast<float>(size) / static_cast<float>(need_w),
                                                static_cast<float>(size) / static_cast<float>(need_h)));
    // We draw in an offscreen at 1:1 glyph then nearest-neighbor scale if needed.
    // Simpler: shrink effective char count so columns fit without fractional glyphs.
    int chars_per_line = kColChars;
    if (need_w > size) {
      const int avail = size - 2 * pad - (ncols - 1) * gap_px;
      chars_per_line = std::max(8, avail / (ncols * font5x7::kCellW));
    }
    const int draw_col_px = chars_per_line * font5x7::kCellW;
    const int total_w = ncols * draw_col_px + (ncols - 1) * gap_px;
    const int origin_x0 = std::max(0, (size - total_w) / 2);
    const int origin_y0 = pad;

    std::vector<std::string> windows[3];
    auto take = [&](int col, std::size_t begin) {
      // Re-wrap with possibly reduced width.
      std::vector<std::string> lines;
      std::size_t i = begin;
      const std::size_t end = text.size();
      while (i < end && static_cast<int>(lines.size()) < max_lines) {
        std::size_t limit = std::min(i + static_cast<std::size_t>(chars_per_line), end);
        std::size_t nl = text.find('\n', i);
        if (nl != std::string::npos && nl < limit) {
          lines.push_back(text.substr(i, nl - i));
          i = nl + 1;
          continue;
        }
        lines.push_back(text.substr(i, limit - i));
        i = limit;
        if (i < end && text[i] == '\n') {
          ++i;
        }
      }
      windows[col] = std::move(lines);
    };

    if (ncols == 1) {
      take(0, 0);
    } else if (ncols == 2) {
      take(0, 0);
      const std::size_t mid = len > per_col ? len - std::min(len, per_col) : len / 2;
      take(1, mid);
    } else {
      take(0, 0);
      const std::size_t mid_start =
          len > per_col ? (len / 2) - std::min(len / 2, per_col / 2) : 0;
      take(1, mid_start);
      const std::size_t end_start = len > per_col ? len - std::min(len, per_col) : 0;
      take(2, end_start);
    }

    (void)scale;
    std::vector<unsigned char> rgb(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 3,
                                   255); // white

    for (int c = 0; c < ncols; ++c) {
      const int ox = origin_x0 + c * (draw_col_px + gap_px);
      draw_column(rgb, size, ox, origin_y0, windows[c], max_lines);
    }

    // Faint vertical rules between columns.
    if (ncols > 1) {
      for (int c = 1; c < ncols; ++c) {
        const int x = origin_x0 + c * (draw_col_px + gap_px) - gap_px / 2 - 1;
        if (x < 0 || x >= size) {
          continue;
        }
        for (int y = pad; y < size - pad; ++y) {
          const std::size_t o =
              (static_cast<std::size_t>(y) * static_cast<std::size_t>(size) + static_cast<std::size_t>(x))
              * 3;
          rgb[o + 0] = rgb[o + 1] = rgb[o + 2] = 220;
        }
      }
    }

    png_write::write_rgb_file(output, size, rgb);
  } catch (const std::exception& ex) {
    std::cerr << "dirtoo-text-thumb: " << ex.what() << '\n';
    return 1;
  }
  return 0;
}
