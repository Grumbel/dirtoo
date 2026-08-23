// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/file_info.hpp"

#include <QDialog>

#include <vector>

namespace dirtoo::app {

void show_properties_dialog(QWidget* parent, const std::vector<fs::FileInfo>& items);

} // namespace dirtoo::app
