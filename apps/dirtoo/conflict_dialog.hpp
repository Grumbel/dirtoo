// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "conflict_probe.hpp"
#include "dirops/ops.hpp"

#include <QString>

#include <filesystem>
#include <optional>

class QWidget;

namespace dirtoo::app {

struct ConflictDecision {
  dirops::ConflictPolicy policy = dirops::ConflictPolicy::Fail;
  bool apply_to_all = false;
};

/// Ask how to resolve a name conflict. Pass source/dest paths when known for size/mtime.
/// Returns nullopt if the user cancels.
/// Modal Replace/Rename/Skip prompt. Performs no filesystem I/O itself (beyond
/// reading cached thumbnails from the local XDG cache): sizes, times and the
/// folder check come from @p probe, gathered beforehand on a worker thread.
[[nodiscard]] std::optional<ConflictDecision> ask_conflict_policy(
    QWidget* parent,
    const QString& destination_name,
    const std::filesystem::path& source_path,
    const std::filesystem::path& destination_path,
    const ConflictProbe& probe);

} // namespace dirtoo::app
