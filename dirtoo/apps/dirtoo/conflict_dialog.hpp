// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirops/ops.hpp"

#include <QDialog>
#include <QString>

#include <optional>

class QWidget;

namespace dirtoo::app {

/// Ask the user how to resolve a name conflict. Returns nullopt if cancelled.
[[nodiscard]] std::optional<dirops::ConflictPolicy> ask_conflict_policy(
    QWidget* parent,
    const QString& destination_name);

} // namespace dirtoo::app
