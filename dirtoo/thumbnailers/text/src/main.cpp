// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// dirtoo-text-thumb — layout-preview thumbnail for text files (Qt QPainter).

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kColChars = 80;
constexpr std::size_t kMaxBytes = 512u * 1024u;

struct Options {
  std::string input;
  std::string output;
  int size = 128;
  QColor fg{0x29, 0x27, 0x20}; // #292720
  QColor bg{0xF5, 0xF0, 0xD8}; // #F5F0D8
  int ss = 4;
  // Pixel size before supersampling. Fractional so --size 128 can use e.g. 2.0
  // while --size 256 uses 4.0; QFont::setPixelSize only takes int, so we round
  // after multiplying by --ss.
  double font_size = 2.0;
  QString font_family = QStringLiteral("DejaVu Sans Mono");
  QString font_weight = QStringLiteral("regular");
};

void usage(const char* argv0)
{
  std::cerr
      << "Usage: " << argv0 << " [options] <input> <output.png> [size]\n"
      << "\n"
      << "  Text layout thumbnail via QPainter (1–3 columns: start/mid/end).\n"
      << "\n"
      << "Options:\n"
      << "  --fg COLOR         Foreground (text) color   (default #292720)\n"
      << "  --bg COLOR         Background color          (default #F5F0D8)\n"
      << "  --font FAMILY      Font family               (default DejaVu Sans Mono)\n"
      << "  --weight NAME      light|regular|medium|bold (default regular)\n"
      << "  --font-size F      Glyph pixel size (float)  (default 2, before --ss)\n"
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

bool parse_color(std::string_view s, QColor& out)
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
    out = QColor(QString::fromUtf8(s.data(), static_cast<int>(s.size())));
    return out.isValid();
  }
  int r = 0, g = 0, b = 0;
  if (std::sscanf(std::string(s).c_str(), "%d,%d,%d", &r, &g, &b) != 3) {
    return false;
  }
  out = QColor(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
  return out.isValid();
}

const char* env_or_null(const char* name)
{
  const char* v = std::getenv(name);
  return (v != nullptr && v[0] != '\0') ? v : nullptr;
}

QFont::Weight weight_from_name(const QString& w)
{
  const QString lower = w.toLower();
  if (lower == QLatin1String("thin") || lower == QLatin1String("ultralight")) {
    return QFont::Thin;
  }
  if (lower == QLatin1String("light") || lower == QLatin1String("book")) {
    return QFont::Light;
  }
  if (lower == QLatin1String("medium") || lower == QLatin1String("demi")
      || lower == QLatin1String("semibold")) {
    return QFont::Medium;
  }
  if (lower == QLatin1String("bold")) {
    return QFont::Bold;
  }
  if (lower == QLatin1String("black") || lower == QLatin1String("heavy")) {
    return QFont::Black;
  }
  return QFont::Normal;
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
    opt.font_family = QString::fromLocal8Bit(v);
  }
  if (const char* v = env_or_null("DIRTOO_TEXT_THUMB_WEIGHT")) {
    opt.font_weight = QString::fromLocal8Bit(v);
  }
  if (const char* v = env_or_null("DIRTOO_TEXT_THUMB_FONT_SIZE")) {
    try {
      opt.font_size = std::stod(v);
    } catch (...) {
      err = std::string("invalid DIRTOO_TEXT_THUMB_FONT_SIZE: ") + v;
      return false;
    }
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
      opt.font_family = QString::fromLocal8Bit(v);
    } else if (a == "--weight") {
      const char* v = need("--weight");
      if (!v) {
        return false;
      }
      opt.font_weight = QString::fromLocal8Bit(v);
    } else if (a == "--font-size") {
      const char* v = need("--font-size");
      if (!v) {
        return false;
      }
      try {
        opt.font_size = std::stod(v);
      } catch (...) {
        err = "invalid --font-size";
        return false;
      }
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
  // Allow sub-integer sizes so --size 128 can match the relative density of
  // --size 256 --font-size 4 via --font-size 2 (or 1.5, etc.). Floor was 3,
  // which made small thumbs impossible to tune.
  if (!(opt.font_size > 0.0) || opt.font_size > 48.0) {
    err = "font-size must be in (0, 48]";
    return false;
  }
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

QFont make_font(const Options& opt, int pixel_size)
{
  QFont font(opt.font_family);
  // Prefer listed compact monospace families if the primary is missing.
  font.setStyleHint(QFont::Monospace);
  font.setFixedPitch(true);
  font.setWeight(weight_from_name(opt.font_weight));
  font.setPixelSize(pixel_size);
  font.setHintingPreference(QFont::PreferFullHinting);
  font.setStyleStrategy(static_cast<QFont::StyleStrategy>(QFont::PreferAntialias | QFont::PreferQuality));
  return font;
}

std::vector<QString> wrap_window(const std::string& text, std::size_t begin, int max_lines,
                                 int chars_per_line)
{
  std::vector<QString> lines;
  std::size_t i = begin;
  const std::size_t end = text.size();
  while (i < end && static_cast<int>(lines.size()) < max_lines) {
    std::size_t limit = std::min(i + static_cast<std::size_t>(chars_per_line), end);
    std::size_t nl = text.find('\n', i);
    if (nl != std::string::npos && nl < limit) {
      lines.push_back(QString::fromLatin1(text.data() + i, static_cast<int>(nl - i)));
      i = nl + 1;
      continue;
    }
    lines.push_back(QString::fromLatin1(text.data() + i, static_cast<int>(limit - i)));
    i = limit;
    if (i < end && text[i] == '\n') {
      ++i;
    }
  }
  return lines;
}

} // namespace

int main(int argc, char** argv)
{
  // QPainter needs a QGuiApplication (no widgets).
  qputenv("QT_QPA_PLATFORM", qgetenv("QT_QPA_PLATFORM").isEmpty() ? QByteArray("offscreen")
                                                                  : qgetenv("QT_QPA_PLATFORM"));
  QGuiApplication app(argc, argv);
  QGuiApplication::setApplicationName(QStringLiteral("dirtoo-text-thumb"));

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
    // QFont::setPixelSize is integer-only; round after supersample so a
    // fractional --font-size (e.g. 2.0 at size 128) still yields a usable
    // hi-res glyph size.
    const int pixel_size =
        std::max(1, static_cast<int>(std::lround(opt.font_size * static_cast<double>(ss))));

    QFont font = make_font(opt, pixel_size);
    // Fallbacks when the requested family is not installed.
    {
      QFont f2 = font;
      f2.setFamilies(QStringList{
          opt.font_family,
          QStringLiteral("DejaVu Sans Mono"),
          QStringLiteral("Liberation Mono"),
          QStringLiteral("JetBrains Mono"),
          QStringLiteral("IBM Plex Mono"),
          QStringLiteral("Nimbus Mono PS"),
          QStringLiteral("monospace"),
      });
      font = f2;
      font.setPixelSize(pixel_size);
      font.setWeight(weight_from_name(opt.font_weight));
      font.setFixedPitch(true);
    }

    const QFontMetrics fm(font);
    const int cell_w = std::max(2, fm.horizontalAdvance(QLatin1Char('M')));
    const int cell_h = std::max(2, fm.lineSpacing());

    const int pad = 2 * ss;
    const int usable_h = std::max(1, hi - 2 * pad);
    const int max_lines = std::max(1, usable_h / cell_h);

    const int col_px = kColChars * cell_w;
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
      chars_per_line = std::max(8, avail / (ncols * cell_w));
    }
    const int draw_col_px = chars_per_line * cell_w;
    const int total_w = ncols * draw_col_px + (ncols - 1) * gap_px;
    const int origin_x0 = std::max(0, (hi - total_w) / 2);
    const int origin_y0 = pad;

    std::vector<std::vector<QString>> windows(static_cast<std::size_t>(ncols));
    if (ncols == 1) {
      windows[0] = wrap_window(text, 0, max_lines, chars_per_line);
    } else if (ncols == 2) {
      windows[0] = wrap_window(text, 0, max_lines, chars_per_line);
      const std::size_t mid = len > per_col ? len - std::min(len, per_col) : len / 2;
      windows[1] = wrap_window(text, mid, max_lines, chars_per_line);
    } else {
      windows[0] = wrap_window(text, 0, max_lines, chars_per_line);
      const std::size_t mid_start =
          len > per_col ? (len / 2) - std::min(len / 2, per_col / 2) : 0;
      windows[1] = wrap_window(text, mid_start, max_lines, chars_per_line);
      const std::size_t end_start = len > per_col ? len - std::min(len, per_col) : 0;
      windows[2] = wrap_window(text, end_start, max_lines, chars_per_line);
    }

    QImage hi_img(hi, hi, QImage::Format_RGB32);
    hi_img.fill(opt.bg);

    {
      QPainter painter(&hi_img);
      painter.setRenderHint(QPainter::TextAntialiasing, true);
      painter.setRenderHint(QPainter::Antialiasing, true);
      painter.setFont(font);
      painter.setPen(opt.fg);

      for (int c = 0; c < ncols; ++c) {
        const int ox = origin_x0 + c * (draw_col_px + gap_px);
        const auto& lines = windows[static_cast<std::size_t>(c)];
        for (int li = 0; li < static_cast<int>(lines.size()) && li < max_lines; ++li) {
          const int y = origin_y0 + li * cell_h + fm.ascent();
          // Crop to column width.
          const QString clipped = lines[static_cast<std::size_t>(li)].left(chars_per_line);
          painter.drawText(ox, y, clipped);
        }
      }

      if (ncols > 1) {
        QColor div = opt.fg;
        div.setAlpha(40);
        painter.setPen(QPen(div, std::max(1, ss / 2)));
        for (int c = 1; c < ncols; ++c) {
          const int x = origin_x0 + c * (draw_col_px + gap_px) - gap_px / 2;
          painter.drawLine(x, pad, x, hi - pad);
        }
      }
    }

    const QImage out = (ss == 1) ? hi_img
                                 : hi_img.scaled(size, size, Qt::IgnoreAspectRatio,
                                                 Qt::SmoothTransformation);
    if (!out.save(QString::fromStdString(opt.output), "PNG")) {
      throw std::runtime_error("failed to write PNG: " + opt.output);
    }
  } catch (const std::exception& ex) {
    std::cerr << "dirtoo-text-thumb: " << ex.what() << '\n';
    return 1;
  }
  return 0;
}
