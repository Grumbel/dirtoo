// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/collection/sorter.hpp"

#include <QString>

namespace dirtoo::app {

/// Map Preferences / QSettings `ui/sort_key` strings ↔ collection::SortKey.
[[nodiscard]] collection::SortKey sort_key_from_settings_string(const QString& s);
[[nodiscard]] QString sort_key_to_settings_string(collection::SortKey key);

} // namespace dirtoo::app
