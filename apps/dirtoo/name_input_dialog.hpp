// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <optional>

class QWidget;

namespace dirtoo::app {

/// Dedicated name entry dialog (rename / new folder / new file).
/// Returns nullopt on cancel. Rejects empty names and names containing '/'.
[[nodiscard]] std::optional<QString> ask_item_name(QWidget* parent,
                                                   const QString& title,
                                                   const QString& label,
                                                   const QString& initial,
                                                   const QString& accept_button = QStringLiteral("OK"));

} // namespace dirtoo::app
