// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// dirtoo-text-thumb — layout-preview thumbnail for text files.
// White/black by default; colors, glyph scale, and size are configurable
// via flags or environment variables (see --help).

#include "font5x7.hpp"
#include "png_write.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kColChars = 80;
constexpr std::size_t kMaxBytes = 512u * 1024u;

struct Rgb {
  unsigned char r = 0;
  unsigned char g = 0;
  unsigned char b = 0;
};

struct Options {
  std::string input;
  std::string output;
  int size = 128;
  Rgb fg{0, 0, 0};
  Rgb bg{255, 255, 255};
  /// Glyph cell scale relative to the full 6×8 cell (1.0 = full, 0.5 = half).
  float scale = 0.5f;
};

void usage(const char* argv0)
{
  std::cerr
      << "Usage: " << argv0 << " [options] <input> <output.png> [size]\n"
      << "\n"
      << "  Text layout thumbnail (1–3 columns of ~80 chars: start/mid/end).\n"
      << "\n"
      << "Options:\n"
      << "  --fg COLOR       Foreground (text) color  (default #000000)\n"
      << "  --bg COLOR       Background color         (default #ffffff)\n"
      << "  --scale F        Glyph scale 0.25–2.0     (default 0.5)\n"
      << "  -h, --help\n"
      << "\n"
      << "COLOR: #rgb, #rrggbb, or R,G,B (0–255).\n"
      << "\n"
      << "Environment (used when the matching flag is omitted):\n"
      << "  DIRTOO_TEXT_THUMB_FG\n"
      << "  DIRTOO_TEXT_THUMB_BG\n"
      << "  DIRTOO_TEXT_THUMB_SCALE\n"
      << "  DIRTOO_TEXT_THUMB_SIZE   (if the size argument is omitted)\n"
      << "\n"
      << "XDG thumbnailer: size is passed as %s; customize colors via env or by\n"
      << "editing share/thumbnailers/text-layout.thumbnailer Exec=…\n";
}

bool parse_hex_digit(char c, int& v)
{
  if (c >= '0' && c <= '9') {
    v = c - '0';
    return true;
  }
  if (c >= 'a' && c <= 'f') {
    v = 10 + (c - 'a');
    return true;
  }
  if (c >= 'A' && c <= 'F') {
    v = 10 + (c - 'A');
    return true;
  }
  return false;
}

bool parse_color(std::string_view s, Rgb& out)
{
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
    s.remove_prefix(1);
  }
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
    s.remove_suffix(1);
  }
  if (s.empty()) {
    return false;
  }
  if (s.front() == '#') {
    s.remove_prefix(1);
    if (s.size() == 3) {
      int r = 0, g = 0, b = 0;
      if (!parse_hex_digit(s[0], r) || !parse_hex_digit(s[1], g) || !parse_hex_digit(s[2], b)) {
        return false;
      }
      out = {static_cast<unsigned char>(r * 17), static_cast<unsigned char>(g * 17),
             static_cast<unsigned char>(b * 17)};
      return true;
    }
    if (s.size() == 6) {
      int v[6];
      for (int i = 0; i < 6; ++i) {
        if (!parse_hex_digit(s[static_cast<std::size_t>(i)], v[i])) {
          return false;
        }
      }
      out = {static_cast<unsigned char>((v[0] << 4) | v[1]),
             static_cast<unsigned char>((v[2] << 4) | v[3]),
             static_cast<unsigned char>((v[4] << 4) | v[5])};
      return true;
    }
    return false;
  }
  // R,G,B
  int r = 0, g = 0, b = 0;
  if (std::sscanf(std::string(s).c_str(), "%d,%d,%d", &r, &g, &b) != 3) {
    return false;
  }
  auto clip = [](int x) { return static_cast<unsigned char>(std::clamp(x, 0, 255)); };
  out = {clip(r), clip(g), clip(b)};
  return true;
}

const char* env_or_null(const char* name)
{
  const char* v = std::getenv(name);
  return (v != nullptr && v[0] != '\0') ? v : nullptr;
}

bool parse_args(int argc, char** argv, Options& opt, std::string& err)
{
  // Env defaults first; flags override.
  if (const char* v = env_or_null("DIRTOO_TEXT_THUMB_FG")) {
    if (!parse_color(v, opt.fg)) {
      err = std::string("invalid DIRTOO_TEXT_THUMB_FG: ") + v;
      return false;
    }
  }
  if (const char* v = env_or_null("DIRTOO_TEXT_THUMB_BG")) {
    if (!parse_color(v, opt.bg)) {
      err = std::string("invalid DIRTOO_TEXT_THUMB_BG: ") + v;
      return false;
    }
  }
  if (const char* v = env_or_null("DIRTOO_TEXT_THUMB_SCALE")) {
    opt.scale = std::strtof(v, nullptr);
  }
  if (const char* v = env_or_null("DIRTOO_TEXT_THUMB_SIZE")) {
    opt.size = std::atoi(v);
  }

  std::vector<std::string> pos;
  for (int i = 1; i < argc; ++i) {
    const std::string_view a{argv[i]};
    if (a == "-h" || a == "--help") {
      usage(argv[0]);
      std::exit(0);
    }
    auto need = [&](const char* name) -> const char* {
      if (i + 1 >= argc) {
        err = std::string(name) + " requires an argument";
        return nullptr;
      }
      return argv[++i];
    };
    if (a == "--fg") {
      const char* v = need("--fg");
      if (v == nullptr) {
        return false;
      }
      if (!parse_color(v, opt.fg)) {
        err = std::string("invalid --fg color: ") + v;
        return false;
      }
    } else if (a == "--bg") {
      const char* v = need("--bg");
      if (v == nullptr) {
        return false;
      }
      if (!parse_color(v, opt.bg)) {
        err = std::string("invalid --bg color: ") + v;
        return false;
      }
    } else if (a == "--scale") {
      const char* v = need("--scale");
      if (v == nullptr) {
        return false;
      }
      opt.scale = std::strtof(v, nullptr);
    } else if (a == "--size") {
      const char* v = need("--size");
      if (v == nullptr) {
        return false;
      }
      opt.size = std::atoi(v);
    } else if (!a.empty() && a.front() == '-') {
      err = std::string("unknown option: ") + argv[i];
      return false;
    } else {
      pos.emplace_back(argv[i]);
    }
  }

  if (pos.size() < 2) {
    err = "need <input> <output.png>";
    return false;
  }
  opt.input = pos[0];
  opt.output = pos[1];
  if (pos.size() >= 3) {
    opt.size = std::atoi(pos[2].c_str());
  }

  opt.scale = std::clamp(opt.scale, 0.25f, 2.0f);
  opt.size = std::clamp(opt.size, 32, 1024);
  return true;
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
  std::string out;
  out.reserve(s.size());
  for (unsigned char ch : s) {
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n' || ch == '\t' || (ch >= 32 && ch < 127)) {
      out.push_back(static_cast<char>(ch == '\t' ? ' ' : ch));
    } else if (ch >= 128) {
      out.push_back('.');
    } else {
      out.push_back(' ');
    }
  }
  return out;
}

struct CellMetrics {
  int cell_w = 3;
  int cell_h = 4;
};

CellMetrics metrics_for_scale(float scale)
{
  // Full cell is 6×8; scale multiplies that (0.5 → 3×4).
  CellMetrics m;
  m.cell_w = std::max(2, static_cast<int>(std::lround(static_cast<double>(font5x7::kCellW) * scale)));
  m.cell_h = std::max(2, static_cast<int>(std::lround(static_cast<double>(font5x7::kCellH) * scale)));
  return m;
}

void plot_char(std::vector<unsigned char>& rgb, int size, int px, int py, unsigned char ch,
               const CellMetrics& cell, const Rgb& fg)
{
  const std::uint8_t* g = font5x7::glyph(ch);
  for (int dy = 0; dy < cell.cell_h; ++dy) {
    const int row = std::min(font5x7::kH - 1, (dy * font5x7::kH) / cell.cell_h);
    const std::uint8_t bits = g[row];
    for (int dx = 0; dx < cell.cell_w; ++dx) {
      // Leave a 1px gap column when cell is wide enough (matches full-size gap).
      const int glyph_cols = std::max(1, cell.cell_w - (cell.cell_w >= 3 ? 1 : 0));
      if (dx >= glyph_cols) {
        continue;
      }
      const int col = std::min(font5x7::kW - 1, (dx * font5x7::kW) / glyph_cols);
      if ((bits & (1u << (font5x7::kW - 1 - col))) == 0) {
        continue;
      }
      const int x = px + dx;
      const int y = py + dy;
      if (x < 0 || y < 0 || x >= size || y >= size) {
        continue;
      }
      const std::size_t o =
          (static_cast<std::size_t>(y) * static_cast<std::size_t>(size) + static_cast<std::size_t>(x))
          * 3;
      rgb[o + 0] = fg.r;
      rgb[o + 1] = fg.g;
      rgb[o + 2] = fg.b;
    }
  }
}

void draw_column(std::vector<unsigned char>& rgb, int size, int origin_x, int origin_y,
                 const std::vector<std::string>& lines, int max_lines, const CellMetrics& cell,
                 const Rgb& fg)
{
  for (int li = 0; li < static_cast<int>(lines.size()) && li < max_lines; ++li) {
    const std::string& line = lines[static_cast<std::size_t>(li)];
    const int y = origin_y + li * cell.cell_h;
    const int n = std::min(static_cast<int>(line.size()), kColChars);
    for (int ci = 0; ci < n; ++ci) {
      const int x = origin_x + ci * cell.cell_w;
      plot_char(rgb, size, x, y, static_cast<unsigned char>(line[static_cast<std::size_t>(ci)]), cell,
                fg);
    }
  }
}

} // namespace

int main(int argc, char** argv)
{
  Options opt;
  std::string err;
  if (!parse_args(argc, argv, opt, err)) {
    if (!err.empty()) {
      std::cerr << "dirtoo-text-thumb: " << err << '\n';
    }
    usage(argv[0]);
    return 2;
  }

  try {
    const std::string text = read_text_capped(opt.input);
    const CellMetrics cell = metrics_for_scale(opt.scale);
    const int size = opt.size;

    const int pad = 2;
    const int usable_h = std::max(1, size - 2 * pad);
    const int max_lines = std::max(1, usable_h / cell.cell_h);

    const int col_px = kColChars * cell.cell_w;
    const int gap_px = 3;

    const std::size_t per_col = static_cast<std::size_t>(max_lines) * static_cast<std::size_t>(kColChars);
    const std::size_t len = text.size();

    int ncols = 1;
    if (len > per_col * 2) {
      ncols = 3;
    } else if (len > per_col) {
      ncols = 2;
    }

    const int need_w = ncols * col_px + (ncols - 1) * gap_px + 2 * pad;
    int chars_per_line = kColChars;
    if (need_w > size) {
      const int avail = size - 2 * pad - (ncols - 1) * gap_px;
      chars_per_line = std::max(8, avail / (ncols * cell.cell_w));
    }
    const int draw_col_px = chars_per_line * cell.cell_w;
    const int total_w = ncols * draw_col_px + (ncols - 1) * gap_px;
    const int origin_x0 = std::max(0, (size - total_w) / 2);
    const int origin_y0 = pad;

    std::vector<std::string> windows[3];
    auto take = [&](int col, std::size_t begin) {
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

    std::vector<unsigned char> rgb(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 3);
    for (std::size_t i = 0; i < rgb.size(); i += 3) {
      rgb[i + 0] = opt.bg.r;
      rgb[i + 1] = opt.bg.g;
      rgb[i + 2] = opt.bg.b;
    }

    for (int c = 0; c < ncols; ++c) {
      const int ox = origin_x0 + c * (draw_col_px + gap_px);
      draw_column(rgb, size, ox, origin_y0, windows[c], max_lines, cell, opt.fg);
    }

    if (ncols > 1) {
      // Divider: midpoint between fg and bg.
      const Rgb div{static_cast<unsigned char>((static_cast<int>(opt.fg.r) + opt.bg.r * 4) / 5),
                    static_cast<unsigned char>((static_cast<int>(opt.fg.g) + opt.bg.g * 4) / 5),
                    static_cast<unsigned char>((static_cast<int>(opt.fg.b) + opt.bg.b * 4) / 5)};
      for (int c = 1; c < ncols; ++c) {
        const int x = origin_x0 + c * (draw_col_px + gap_px) - gap_px / 2 - 1;
        if (x < 0 || x >= size) {
          continue;
        }
        for (int y = pad; y < size - pad; ++y) {
          const std::size_t o =
              (static_cast<std::size_t>(y) * static_cast<std::size_t>(size) + static_cast<std::size_t>(x))
              * 3;
          rgb[o + 0] = div.r;
          rgb[o + 1] = div.g;
          rgb[o + 2] = div.b;
        }
      }
    }

    png_write::write_rgb_file(opt.output, size, rgb);
  } catch (const std::exception& ex) {
    std::cerr << "dirtoo-text-thumb: " << ex.what() << '\n';
    return 1;
  }
  return 0;
}
