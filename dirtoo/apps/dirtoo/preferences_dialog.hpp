// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app_settings.hpp"

#include <QDialog>

namespace dirtoo::app {

/// Simple preferences surface for AppSettings keys.
[[nodiscard]] bool show_preferences_dialog(QWidget* parent, AppSettings* settings);

} // namespace dirtoo::app
