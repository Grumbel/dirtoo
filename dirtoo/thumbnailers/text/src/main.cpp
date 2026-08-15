// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// dirtoo-text-thumb — layout-preview thumbnail for text files.
// FreeType + Fontconfig for family/weight/size; supersampled downscale.

#include "png_write.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <fontconfig/fontconfig.h>

#include <algorithm>
#include <cctype>
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
  int ss = 4;
  /// Logical pixel size of the font (before supersampling).
  int font_size = 6;
  std::string font_family = "JetBrains Mono";
  /// light | regular | medium | bold (default regular)
  std::string font_weight = "regular";
};

void usage(const char* argv0)
{
  std::cerr
      << "Usage: " << argv0 << " [options] <input> <output.png> [size]\n"
      << "\n"
      << "  Text layout thumbnail (1–3 columns of ~80 chars: start/mid/end).\n"
      << "\n"
      << "Options:\n"
      << "  --fg COLOR         Foreground (text) color   (default #000000)\n"
      << "  --bg COLOR         Background color          (default #ffffff)\n"
      << "  --font FAMILY      Font family               (default JetBrains Mono)\n"
      << "  --weight NAME      light|regular|medium|bold (default regular)\n"
      << "  --font-size N      Glyph pixel size          (default 6, before --ss)\n"
      << "  --ss N             Supersample factor 1–8    (default 4)\n"
      << "  --size N           Thumbnail edge pixels     (default 128)\n"
      << "  -h, --help\n"
      << "\n"
      << "COLOR: #rgb, #rrggbb, or R,G,B (0–255).\n"
      << "\n"
      << "Environment (when the matching flag is omitted):\n"
      << "  DIRTOO_TEXT_THUMB_FG  DIRTOO_TEXT_THUMB_BG\n"
      << "  DIRTOO_TEXT_THUMB_FONT  DIRTOO_TEXT_THUMB_WEIGHT\n"
      << "  DIRTOO_TEXT_THUMB_FONT_SIZE  DIRTOO_TEXT_THUMB_SS\n"
      << "  DIRTOO_TEXT_THUMB_SIZE\n";
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

int fc_weight_from_name(std::string_view w)
{
  std::string lower(w);
  for (char& c : lower) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (lower == "thin" || lower == "ultralight") {
    return FC_WEIGHT_THIN;
  }
  if (lower == "light" || lower == "book") {
    return FC_WEIGHT_LIGHT;
  }
  if (lower == "medium" || lower == "demi" || lower == "semibold") {
    return FC_WEIGHT_MEDIUM;
  }
  if (lower == "bold") {
    return FC_WEIGHT_BOLD;
  }
  if (lower == "black" || lower == "heavy") {
    return FC_WEIGHT_BLACK;
  }
  return FC_WEIGHT_REGULAR;
}

bool parse_args(int argc, char** argv, Options& opt, std::string& err)
{
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
  if (const char* v = env_or_null("DIRTOO_TEXT_THUMB_FONT")) {
    opt.font_family = v;
  }
  if (const char* v = env_or_null("DIRTOO_TEXT_THUMB_WEIGHT")) {
    opt.font_weight = v;
  }
  if (const char* v = env_or_null("DIRTOO_TEXT_THUMB_FONT_SIZE")) {
    opt.font_size = std::atoi(v);
  }
  if (const char* v = env_or_null("DIRTOO_TEXT_THUMB_SS")) {
    opt.ss = std::atoi(v);
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
      if (!v || !parse_color(v, opt.fg)) {
        err = err.empty() ? std::string("invalid --fg") : err;
        return false;
      }
    } else if (a == "--bg") {
      const char* v = need("--bg");
      if (!v || !parse_color(v, opt.bg)) {
        err = err.empty() ? std::string("invalid --bg") : err;
        return false;
      }
    } else if (a == "--font") {
      const char* v = need("--font");
      if (!v) {
        return false;
      }
      opt.font_family = v;
    } else if (a == "--weight") {
      const char* v = need("--weight");
      if (!v) {
        return false;
      }
      opt.font_weight = v;
    } else if (a == "--font-size") {
      const char* v = need("--font-size");
      if (!v) {
        return false;
      }
      opt.font_size = std::atoi(v);
    } else if (a == "--ss") {
      const char* v = need("--ss");
      if (!v) {
        return false;
      }
      opt.ss = std::atoi(v);
    } else if (a == "--size") {
      const char* v = need("--size");
      if (!v) {
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

  opt.size = std::clamp(opt.size, 32, 1024);
  opt.ss = std::clamp(opt.ss, 1, 8);
  opt.font_size = std::clamp(opt.font_size, 3, 48);
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

/// Resolve family+weight to a font file path via Fontconfig.
std::string resolve_font_file(const std::string& family, const std::string& weight)
{
  if (!FcInit()) {
    throw std::runtime_error("fontconfig init failed");
  }

  // Prefer an explicit file path if the user passed one.
  if (family.find('/') != std::string::npos || family.ends_with(".ttf") || family.ends_with(".otf")
      || family.ends_with(".ttc")) {
    return family;
  }

  const int fc_w = fc_weight_from_name(weight);
  const char* fallbacks[] = {
      family.c_str(),
      "JetBrains Mono",
      "IBM Plex Mono",
      "DejaVu Sans Mono",
      "Nimbus Mono PS",
      "Liberation Mono",
      "FreeMono",
      "monospace",
  };

  for (const char* fam : fallbacks) {
    FcPattern* pat = FcPatternCreate();
    FcPatternAddString(pat, FC_FAMILY, reinterpret_cast<const FcChar8*>(fam));
    FcPatternAddInteger(pat, FC_WEIGHT, fc_w);
    FcPatternAddString(pat, FC_STYLE, reinterpret_cast<const FcChar8*>("Regular"));
    FcConfigSubstitute(nullptr, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    FcResult result = FcResultNoMatch;
    FcPattern* match = FcFontMatch(nullptr, pat, &result);
    FcPatternDestroy(pat);
    if (match == nullptr) {
      continue;
    }
    FcChar8* file = nullptr;
    if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch && file != nullptr) {
      std::string path(reinterpret_cast<const char*>(file));
      FcPatternDestroy(match);
      return path;
    }
    FcPatternDestroy(match);
  }
  throw std::runtime_error("no font file found for family '" + family + "'");
}

struct FontFace {
  FT_Library library = nullptr;
  FT_Face face = nullptr;
  int pixel_size = 6;
  int cell_w = 4;
  int cell_h = 8;
  int ascent = 6;

  FontFace() = default;
  FontFace(const FontFace&) = delete;
  FontFace& operator=(const FontFace&) = delete;

  ~FontFace()
  {
    if (face) {
      FT_Done_Face(face);
    }
    if (library) {
      FT_Done_FreeType(library);
    }
  }

  void open(const std::string& path, int px)
  {
    if (FT_Init_FreeType(&library) != 0) {
      throw std::runtime_error("FT_Init_FreeType failed");
    }
    if (FT_New_Face(library, path.c_str(), 0, &face) != 0) {
      throw std::runtime_error("cannot open font: " + path);
    }
    pixel_size = px;
    if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(px)) != 0) {
      throw std::runtime_error("FT_Set_Pixel_Sizes failed");
    }
    // Monospace-ish cell from metrics; fall back to max advance.
    const int asc = static_cast<int>((face->size->metrics.ascender + 63) >> 6);
    const int desc = static_cast<int>((-face->size->metrics.descender + 63) >> 6);
    ascent = std::max(1, asc);
    cell_h = std::max(2, asc + desc + 1);
    int adv = static_cast<int>((face->size->metrics.max_advance + 63) >> 6);
    if (adv <= 0) {
      adv = static_cast<int>((face->max_advance_width * px) / std::max(1, static_cast<int>(face->units_per_EM)));
    }
    cell_w = std::max(2, adv);
  }
};

void blend_pixel(std::vector<unsigned char>& rgb, int size, int x, int y, const Rgb& fg,
                 unsigned char coverage)
{
  if (coverage == 0 || x < 0 || y < 0 || x >= size || y >= size) {
    return;
  }
  const std::size_t o =
      (static_cast<std::size_t>(y) * static_cast<std::size_t>(size) + static_cast<std::size_t>(x)) * 3;
  const unsigned a = coverage;
  rgb[o + 0] = static_cast<unsigned char>((fg.r * a + rgb[o + 0] * (255 - a)) / 255);
  rgb[o + 1] = static_cast<unsigned char>((fg.g * a + rgb[o + 1] * (255 - a)) / 255);
  rgb[o + 2] = static_cast<unsigned char>((fg.b * a + rgb[o + 2] * (255 - a)) / 255);
}

void draw_char(FontFace& font, std::vector<unsigned char>& rgb, int size, int pen_x, int baseline_y,
               unsigned char ch, const Rgb& fg)
{
  if (ch < 32 || ch > 126) {
    ch = '?';
  }
  if (FT_Load_Char(font.face, ch, FT_LOAD_RENDER) != 0) {
    return;
  }
  const FT_Bitmap& bm = font.face->glyph->bitmap;
  const int top = font.face->glyph->bitmap_top;
  const int left = font.face->glyph->bitmap_left;
  for (unsigned row = 0; row < bm.rows; ++row) {
    for (unsigned col = 0; col < bm.width; ++col) {
      const unsigned char cov = bm.buffer[row * static_cast<unsigned>(bm.pitch) + col];
      const int x = pen_x + left + static_cast<int>(col);
      const int y = baseline_y - top + static_cast<int>(row);
      blend_pixel(rgb, size, x, y, fg, cov);
    }
  }
}

void draw_column(FontFace& font, std::vector<unsigned char>& rgb, int size, int origin_x, int origin_y,
                 const std::vector<std::string>& lines, int max_lines, const Rgb& fg)
{
  for (int li = 0; li < static_cast<int>(lines.size()) && li < max_lines; ++li) {
    const std::string& line = lines[static_cast<std::size_t>(li)];
    const int baseline = origin_y + li * font.cell_h + font.ascent;
    const int n = std::min(static_cast<int>(line.size()), kColChars);
    for (int ci = 0; ci < n; ++ci) {
      const int x = origin_x + ci * font.cell_w;
      draw_char(font, rgb, size, x, baseline, static_cast<unsigned char>(line[static_cast<std::size_t>(ci)]),
                fg);
    }
  }
}

void downsample_box(const std::vector<unsigned char>& src, int hi, std::vector<unsigned char>& dst,
                    int lo)
{
  const int factor = hi / lo;
  dst.assign(static_cast<std::size_t>(lo) * static_cast<std::size_t>(lo) * 3, 0);
  if (factor <= 1) {
    dst = src;
    return;
  }
  for (int y = 0; y < lo; ++y) {
    for (int x = 0; x < lo; ++x) {
      unsigned sum_r = 0, sum_g = 0, sum_b = 0;
      const int count = factor * factor;
      for (int dy = 0; dy < factor; ++dy) {
        for (int dx = 0; dx < factor; ++dx) {
          const std::size_t o =
              (static_cast<std::size_t>(y * factor + dy) * static_cast<std::size_t>(hi)
               + static_cast<std::size_t>(x * factor + dx))
              * 3;
          sum_r += src[o + 0];
          sum_g += src[o + 1];
          sum_b += src[o + 2];
        }
      }
      const std::size_t d =
          (static_cast<std::size_t>(y) * static_cast<std::size_t>(lo) + static_cast<std::size_t>(x)) * 3;
      dst[d + 0] = static_cast<unsigned char>(sum_r / count);
      dst[d + 1] = static_cast<unsigned char>(sum_g / count);
      dst[d + 2] = static_cast<unsigned char>(sum_b / count);
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
    const int size = opt.size;
    const int ss = opt.ss;
    const int hi = size * ss;

    const std::string font_path = resolve_font_file(opt.font_family, opt.font_weight);
    FontFace font;
    // Render at supersampled pixel size for smooth stems.
    font.open(font_path, opt.font_size * ss);

    const int pad = 2 * ss;
    const int usable_h = std::max(1, hi - 2 * pad);
    const int max_lines = std::max(1, usable_h / font.cell_h);

    const int col_px = kColChars * font.cell_w;
    const int gap_px = 3 * ss;

    const std::size_t per_col =
        static_cast<std::size_t>(max_lines) * static_cast<std::size_t>(kColChars);
    const std::size_t len = text.size();

    int ncols = 1;
    if (len > per_col * 2) {
      ncols = 3;
    } else if (len > per_col) {
      ncols = 2;
    }

    const int need_w = ncols * col_px + (ncols - 1) * gap_px + 2 * pad;
    int chars_per_line = kColChars;
    if (need_w > hi) {
      const int avail = hi - 2 * pad - (ncols - 1) * gap_px;
      chars_per_line = std::max(8, avail / (ncols * font.cell_w));
    }
    const int draw_col_px = chars_per_line * font.cell_w;
    const int total_w = ncols * draw_col_px + (ncols - 1) * gap_px;
    const int origin_x0 = std::max(0, (hi - total_w) / 2);
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

    std::vector<unsigned char> hi_rgb(static_cast<std::size_t>(hi) * static_cast<std::size_t>(hi) * 3);
    for (std::size_t i = 0; i < hi_rgb.size(); i += 3) {
      hi_rgb[i + 0] = opt.bg.r;
      hi_rgb[i + 1] = opt.bg.g;
      hi_rgb[i + 2] = opt.bg.b;
    }

    for (int c = 0; c < ncols; ++c) {
      const int ox = origin_x0 + c * (draw_col_px + gap_px);
      draw_column(font, hi_rgb, hi, ox, origin_y0, windows[c], max_lines, opt.fg);
    }

    if (ncols > 1) {
      const Rgb div{static_cast<unsigned char>((static_cast<int>(opt.fg.r) + opt.bg.r * 4) / 5),
                    static_cast<unsigned char>((static_cast<int>(opt.fg.g) + opt.bg.g * 4) / 5),
                    static_cast<unsigned char>((static_cast<int>(opt.fg.b) + opt.bg.b * 4) / 5)};
      for (int c = 1; c < ncols; ++c) {
        const int x = origin_x0 + c * (draw_col_px + gap_px) - gap_px / 2 - 1;
        if (x < 0 || x >= hi) {
          continue;
        }
        for (int y = pad; y < hi - pad; ++y) {
          const std::size_t o =
              (static_cast<std::size_t>(y) * static_cast<std::size_t>(hi) + static_cast<std::size_t>(x))
              * 3;
          hi_rgb[o + 0] = div.r;
          hi_rgb[o + 1] = div.g;
          hi_rgb[o + 2] = div.b;
        }
      }
    }

    std::vector<unsigned char> rgb;
    downsample_box(hi_rgb, hi, rgb, size);
    png_write::write_rgb_file(opt.output, size, rgb);
  } catch (const std::exception& ex) {
    std::cerr << "dirtoo-text-thumb: " << ex.what() << '\n';
    return 1;
  }
  return 0;
}
