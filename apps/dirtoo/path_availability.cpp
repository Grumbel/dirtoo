// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "path_availability.hpp"

#include "async_io.hpp"

#include <QAction>
#include <QCoreApplication>
#include <QStringList>
#include <QVariant>

#include <system_error>

namespace dirtoo::app {
namespace {

constexpr const char* kPathsProperty = "dirtoo_availability_paths";
constexpr const char* kToolTipProperty = "dirtoo_availability_tooltip";

} // namespace

PathAvailability& PathAvailability::instance()
{
  // Child of qApp: destroyed with the application, which drops any pending
  // continuations instead of running them against a dead object.
  static PathAvailability* self = new PathAvailability(QCoreApplication::instance());
  return *self;
}

PathAvailability::PathAvailability(QObject* parent)
    : QObject(parent)
{
}

void PathAvailability::track(QAction* action, const fs::Location& location)
{
  if (location.is_file() || location.is_archive()) {
    track(action, {location.as_path()});
  }
}

void PathAvailability::track(QAction* action, const std::vector<std::filesystem::path>& paths)
{
  if (action == nullptr || paths.empty()) {
    return;
  }
  QStringList keys;
  for (const auto& p : paths) {
    if (!p.empty()) {
      keys << QString::fromStdString(p.string());
    }
  }
  if (keys.isEmpty()) {
    return;
  }
  action->setProperty(kPathsProperty, keys);
  action->setProperty(kToolTipProperty, action->toolTip());
  apply(action); // last known state, instantly

  for (const QString& key : keys) {
    waiting_[key].append(QPointer<QAction>(action));
    if (in_flight_.contains(key)) {
      continue; // still waiting on an earlier (possibly hung) check
    }
    in_flight_.insert(key);
    run_io(
        this,
        [key] {
          std::error_code ec;
          // symlink_status: a dangling link entry still "exists" as an entry.
          return std::filesystem::exists(
                     std::filesystem::symlink_status(key.toStdString(), ec))
                 && !ec;
        },
        [this, key](bool exists) { on_checked(key, exists); });
  }
}

void PathAvailability::on_checked(const QString& key, bool exists)
{
  in_flight_.remove(key);
  known_.insert(key, exists ? State::Present : State::Missing);
  for (const QPointer<QAction>& action : waiting_.take(key)) {
    if (action) { // menus are rebuilt on every show; old actions are gone
      apply(action);
    }
  }
}

void PathAvailability::apply(QAction* action) const
{
  const QStringList keys = action->property(kPathsProperty).toStringList();
  bool all_missing = !keys.isEmpty();
  for (const QString& key : keys) {
    if (known_.value(key, State::Present) != State::Missing) {
      all_missing = false;
      break;
    }
  }
  action->setEnabled(!all_missing);
  const QString base = action->property(kToolTipProperty).toString();
  if (all_missing) {
    const QString note = QStringLiteral("Not available (missing or not mounted)");
    action->setToolTip(base.isEmpty() ? note : base + QStringLiteral("\n") + note);
  } else {
    action->setToolTip(base);
  }
}

} // namespace dirtoo::app
