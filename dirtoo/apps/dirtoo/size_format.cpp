// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "size_format.hpp"

namespace dirtoo::app {
namespace {

SizeUnitStyle g_style = SizeUnitStyle::Si;

QString format_with_base(std::uint64_t bytes, double base, const char* const* units, int n_units)
{
  double value = static_cast<double>(bytes);
  int unit = 0;
  while (value >= base && unit + 1 < n_units) {
    value /= base;
    ++unit;
  }
  if (unit == 0) {
    return QStringLiteral("%1 B").arg(bytes);
  }
  if (value < 10.0) {
    return QStringLiteral("%1 %2").arg(value, 0, 'f', 1).arg(QLatin1String(units[unit]));
  }
  return QStringLiteral("%1 %2")
      .arg(value, 0, 'f', value < 100.0 ? 1 : 0)
      .arg(QLatin1String(units[unit]));
}

} // namespace

void set_size_unit_style(SizeUnitStyle style)
{
  g_style = style;
}

SizeUnitStyle size_unit_style()
{
  return g_style;
}

SizeUnitStyle size_unit_style_from_string(const QString& s)
{
  const QString lower = s.toLower();
  if (lower == QLatin1String("iec") || lower == QLatin1String("binary")
      || lower == QLatin1String("mib")) {
    return SizeUnitStyle::Iec;
  }
  return SizeUnitStyle::Si;
}

QString size_unit_style_to_string(SizeUnitStyle style)
{
  switch (style) {
  case SizeUnitStyle::Iec:
    return QStringLiteral("iec");
  case SizeUnitStyle::Si:
  default:
    return QStringLiteral("si");
  }
}

QString format_byte_size(std::uint64_t bytes, SizeUnitStyle style)
{
  static constexpr const char* kSi[] = {"B", "kB", "MB", "GB", "TB", "PB"};
  static constexpr const char* kIec[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
  if (style == SizeUnitStyle::Iec) {
    return format_with_base(bytes, 1024.0, kIec, 6);
  }
  return format_with_base(bytes, 1000.0, kSi, 6);
}

} // namespace dirtoo::app
