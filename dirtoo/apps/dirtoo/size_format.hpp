// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <cstdint>

namespace dirtoo::app {

/// How file sizes are shown in the UI.
enum class SizeUnitStyle {
  /// Base 1000: B, KB, MB, GB, TB (dirtoo-py / SI default).
  Si = 0,
  /// Base 1024: B, KiB, MiB, GiB, TiB (IEC).
  Iec = 1,
};

/// Process-wide style used by format_byte_size() (set from Preferences / startup).
void set_size_unit_style(SizeUnitStyle style);
[[nodiscard]] SizeUnitStyle size_unit_style();

[[nodiscard]] SizeUnitStyle size_unit_style_from_string(const QString& s);
[[nodiscard]] QString size_unit_style_to_string(SizeUnitStyle style);

/// Format a byte count with the current (or explicit) unit style.
[[nodiscard]] QString format_byte_size(std::uint64_t bytes,
                                       SizeUnitStyle style = size_unit_style());

} // namespace dirtoo::app
