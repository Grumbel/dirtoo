// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "file_set_controller.hpp"

#include "activity_monitor.hpp"

#include <QMessageBox>

#include <filesystem>
#include <map>
#include <unordered_map>

namespace dirtoo::app {
namespace {

void report_set_error(FileSetController* self, const QString& message, int timeout_ms = 8000)
{
  qWarning().noquote() << QStringLiteral("dirtoo: sets: %1").arg(message);
  ActivityMonitor::instance().append_log(QtWarningMsg, QStringLiteral("Sets: %1").arg(message));
  emit self->status_message(message, timeout_ms);
}

} // namespace

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
    const QString msg =
        QStringLiteral("Cannot open sets database: %1").arg(QString::fromStdString(err));
    report_set_error(this, msg);
    QMessageBox::warning(dialog_parent_, QStringLiteral("Sets"), msg);
    return {};
  }
  auto set = store_.create_set({}, {}, &err);
  if (!set) {
    report_set_error(
        this, QStringLiteral("Could not create set: %1").arg(QString::fromStdString(err)));
    return {};
  }
  std::vector<std::string> keys;
  keys.reserve(selection.size());
  for (const auto& fi : selection) {
    keys.push_back(path_key_for(fi));
  }
  err.clear();
  const int n = store_.add_members(set->id, keys, &err);
  if (!err.empty() && n == 0 && !keys.empty()) {
    report_set_error(this, QStringLiteral("Set created but adding members failed: %1")
                               .arg(QString::fromStdString(err)));
  }
  last_set_id_ = QString::fromStdString(set->id);
  emit set_created(last_set_id_, n);
  emit status_message(QStringLiteral("Set created (%1 file%2)")
                          .arg(n)
                          .arg(n == 1 ? QString() : QStringLiteral("s")),
                      4000);
  return last_set_id_;
}

bool FileSetController::add_selection_to_set(const std::vector<dirtoo::fs::FileInfo>& selection,
                                             const QString& set_id)
{
  if (selection.empty() || set_id.isEmpty()) {
    return false;
  }
  std::string err;
  if (!ensure_store(&err)) {
    report_set_error(this, QStringLiteral("Cannot open sets: %1").arg(QString::fromStdString(err)));
    return false;
  }
  if (!store_.get_set(set_id.toStdString())) {
    report_set_error(this, QStringLiteral("Set no longer exists"));
    return false;
  }
  std::vector<std::string> keys;
  keys.reserve(selection.size());
  for (const auto& fi : selection) {
    keys.push_back(path_key_for(fi));
  }
  err.clear();
  const int n = store_.add_members(set_id.toStdString(), keys, &err);
  last_set_id_ = set_id;
  emit set_updated(set_id, static_cast<int>(store_.member_count(set_id.toStdString())));
  emit status_message(QStringLiteral("Added to set (%1 file%2)")
                          .arg(n)
                          .arg(n == 1 ? QString() : QStringLiteral("s")),
                      4000);
  return true;
}

bool FileSetController::remove_selection_from_set(
    const std::vector<dirtoo::fs::FileInfo>& selection)
{
  if (selection.empty()) {
    emit status_message(QStringLiteral("Select files to remove from a set"), 3000);
    return false;
  }
  std::string err;
  if (!ensure_store(&err)) {
    report_set_error(this, QStringLiteral("Cannot open sets: %1").arg(QString::fromStdString(err)));
    return false;
  }
  int removed = 0;
  std::map<std::string, int> touched; // set_id → remaining count after
  for (const auto& fi : selection) {
    const std::string key = path_key_for(fi);
    const auto sets = store_.sets_for_path(key);
    for (const auto& s : sets) {
      if (store_.remove_member(s.id, key, &err)) {
        ++removed;
        touched[s.id] = static_cast<int>(store_.member_count(s.id));
      }
    }
  }
  if (removed == 0) {
    emit status_message(QStringLiteral("Selection is not in any set"), 3000);
    return false;
  }
  for (const auto& [sid, count] : touched) {
    if (count <= 0) {
      store_.delete_set(sid, &err);
      emit set_dissolved(QString::fromStdString(sid));
      if (last_set_id_ == QString::fromStdString(sid)) {
        last_set_id_.clear();
      }
    } else {
      emit set_updated(QString::fromStdString(sid), count);
    }
  }
  emit status_message(QStringLiteral("Removed %1 file%2 from set")
                          .arg(removed)
                          .arg(removed == 1 ? QString() : QStringLiteral("s")),
                      4000);
  return true;
}

bool FileSetController::add_selection_to_last_set(
    const std::vector<dirtoo::fs::FileInfo>& selection)
{
  if (last_set_id_.isEmpty()) {
    emit status_message(QStringLiteral("No set yet — use Ctrl+G or Create Set"), 4000);
    return false;
  }
  return add_selection_to_set(selection, last_set_id_);
}

QString FileSetController::toggle_set_for_selection(
    const std::vector<dirtoo::fs::FileInfo>& selection)
{
  if (selection.empty()) {
    emit status_message(QStringLiteral("Select files to toggle set membership"), 3000);
    return {};
  }
  std::string err;
  if (!ensure_store(&err)) {
    report_set_error(this,
                     QStringLiteral("Cannot open sets: %1").arg(QString::fromStdString(err)));
    return {};
  }

  struct Entry {
    std::string key;
    std::string set_id; // empty if not in a set
  };
  std::vector<Entry> entries;
  entries.reserve(selection.size());
  std::unordered_map<std::string, int> set_counts;
  int in_set_count = 0;
  for (const auto& fi : selection) {
    Entry e;
    e.key = path_key_for(fi);
    const auto sets = store_.sets_for_path(e.key);
    if (!sets.empty()) {
      e.set_id = sets.front().id;
      ++in_set_count;
      ++set_counts[e.set_id];
    }
    entries.push_back(std::move(e));
  }

  const int n = static_cast<int>(entries.size());

  // All selected files are already in a set → remove them (toggle off).
  if (in_set_count == n) {
    // Prefer one message when all share the same set.
    (void)remove_selection_from_set(selection);
    return {};
  }

  // At least one file has no set → create or extend.
  std::string target_id;
  if (!set_counts.empty()) {
    // Use the set that already covers the most of the selection.
    int best = -1;
    for (const auto& [sid, c] : set_counts) {
      if (c > best) {
        best = c;
        target_id = sid;
      }
    }
  }

  if (target_id.empty()) {
    return create_set_from_selection(selection);
  }

  if (add_selection_to_set(selection, QString::fromStdString(target_id))) {
    return QString::fromStdString(target_id);
  }
  return {};
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
  if (auto by_id = store_.get_set(query)) {
    return by_id;
  }
  // Unique id prefix.
  if (query.size() >= 8) {
    std::optional<dirtoo::sets::FileSet> pref;
    for (const auto& s : store_.list_sets()) {
      if (s.id.size() >= query.size() && s.id.compare(0, query.size(), query) == 0) {
        if (pref) {
          if (error != nullptr) {
            *error = "ambiguous set id prefix";
          }
          return std::nullopt;
        }
        pref = s;
      }
    }
    if (pref) {
      return pref;
    }
  }
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

QStringList FileSetController::member_paths_for_path(const QString& path_key)
{
  QStringList out;
  std::string err;
  if (!ensure_store(&err) || path_key.isEmpty()) {
    return out;
  }
  const auto sets = store_.sets_for_path(path_key.toStdString());
  if (sets.empty()) {
    return out;
  }
  for (const auto& m : store_.members(sets.front().id)) {
    out.append(QString::fromStdString(m.path_key));
  }
  return out;
}

} // namespace dirtoo::app
