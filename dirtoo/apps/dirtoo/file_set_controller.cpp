// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "file_set_controller.hpp"

#include <QMessageBox>

#include <filesystem>

namespace dirtoo::app {

FileSetController::FileSetController(QObject* parent)
    : QObject(parent)
{
}

void FileSetController::set_dialog_parent(QWidget* parent)
{
  dialog_parent_ = parent;
}

bool FileSetController::ensure_store(std::string* error)
{
  if (store_.is_open()) {
    return true;
  }
  return store_.open(dirtoo::sets::FileSetStore::default_path(), error);
}

dirtoo::sets::FileSetStore& FileSetController::store()
{
  std::string err;
  (void)ensure_store(&err);
  return store_;
}

std::string FileSetController::path_key_for(const dirtoo::fs::FileInfo& fi)
{
  if (fi.location().is_archive()) {
    return fi.location().as_url();
  }
  std::error_code ec;
  const auto abs = std::filesystem::absolute(fi.path(), ec);
  if (ec) {
    return fi.path().string();
  }
  return abs.lexically_normal().string();
}

QString FileSetController::create_set_from_selection(
    const std::vector<dirtoo::fs::FileInfo>& selection)
{
  if (selection.empty()) {
    emit status_message(QStringLiteral("Select files to create a set"), 3000);
    return {};
  }
  std::string err;
  if (!ensure_store(&err)) {
    QMessageBox::warning(dialog_parent_, QStringLiteral("Sets"),
                         QStringLiteral("Cannot open sets database:\n%1")
                             .arg(QString::fromStdString(err)));
    return {};
  }
  auto set = store_.create_set({}, {}, &err);
  if (!set) {
    emit status_message(QStringLiteral("Could not create set: %1").arg(QString::fromStdString(err)),
                        5000);
    return {};
  }
  std::vector<std::string> keys;
  keys.reserve(selection.size());
  for (const auto& fi : selection) {
    keys.push_back(path_key_for(fi));
  }
  const int n = store_.add_members(set->id, keys, &err);
  last_set_id_ = QString::fromStdString(set->id);
  emit set_created(last_set_id_, n);
  emit status_message(
      QStringLiteral("Set created (%1 file%2)")
          .arg(n)
          .arg(n == 1 ? QString() : QStringLiteral("s")),
      4000);
  return last_set_id_;
}

bool FileSetController::add_selection_to_last_set(
    const std::vector<dirtoo::fs::FileInfo>& selection)
{
  if (selection.empty()) {
    emit status_message(QStringLiteral("Select files to add to the set"), 3000);
    return false;
  }
  if (last_set_id_.isEmpty()) {
    emit status_message(QStringLiteral("No set yet — create one with Ctrl+G first"), 4000);
    return false;
  }
  std::string err;
  if (!ensure_store(&err)) {
    emit status_message(QStringLiteral("Cannot open sets: %1").arg(QString::fromStdString(err)),
                        5000);
    return false;
  }
  if (!store_.get_set(last_set_id_.toStdString())) {
    emit status_message(QStringLiteral("Last set no longer exists"), 4000);
    last_set_id_.clear();
    return false;
  }
  std::vector<std::string> keys;
  keys.reserve(selection.size());
  for (const auto& fi : selection) {
    keys.push_back(path_key_for(fi));
  }
  const int n = store_.add_members(last_set_id_.toStdString(), keys, &err);
  emit set_updated(last_set_id_, static_cast<int>(store_.member_count(last_set_id_.toStdString())));
  emit status_message(
      QStringLiteral("Added %1 file%2 to set")
          .arg(n)
          .arg(n == 1 ? QString() : QStringLiteral("s")),
      4000);
  return n > 0;
}

std::optional<dirtoo::sets::FileSet>
FileSetController::resolve_query(std::string_view query, std::string* error)
{
  if (!ensure_store(error)) {
    return std::nullopt;
  }
  if (query.empty()) {
    if (error != nullptr) {
      *error = "empty set query";
    }
    return std::nullopt;
  }
  // Prefer exact id.
  if (auto by_id = store_.get_set(query)) {
    return by_id;
  }
  // Unique label match (case-sensitive for now).
  std::optional<dirtoo::sets::FileSet> found;
  for (const auto& s : store_.list_sets()) {
    if (s.label == query) {
      if (found) {
        if (error != nullptr) {
          *error = "ambiguous set label";
        }
        return std::nullopt;
      }
      found = s;
    }
  }
  if (!found && error != nullptr) {
    *error = "set not found";
  }
  return found;
}

} // namespace dirtoo::app
