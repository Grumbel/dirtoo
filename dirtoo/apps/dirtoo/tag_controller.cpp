// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tag_controller.hpp"

#include "activity_monitor.hpp"

#include "dirtoo/hash/checksum_store.hpp"
#include "dirtoo/tags/tag_store.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QCompleter>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStringListModel>
#include <QVBoxLayout>

#include <filesystem>
#include <set>

namespace dirtoo::app {
namespace {

/// Split a free-form tag field on commas and/or whitespace into unique names.
QStringList split_tag_names(const QString& raw)
{
  QStringList out;
  QString cur;
  auto flush = [&] {
    const QString t = cur.trimmed();
    cur.clear();
    if (t.isEmpty() || out.contains(t)) {
      return;
    }
    out << t;
  };
  for (QChar ch : raw) {
    if (ch == QLatin1Char(',') || ch.isSpace()) {
      flush();
    } else {
      cur.append(ch);
    }
  }
  flush();
  return out;
}

QStringList known_tag_names()
{
  QStringList names;
  dirtoo::tags::TagStore store;
  std::string err;
  if (!store.open(dirtoo::tags::TagStore::default_path(), &err)) {
    return names;
  }
  for (const auto& def : store.list_tags()) {
    names << QString::fromStdString(def.name);
  }
  names.sort(Qt::CaseInsensitive);
  return names;
}

std::string path_key_for(const dirtoo::fs::FileInfo& fi)
{
  const std::string path_str = fi.path().string();
  if (path_str.find("://") != std::string::npos || path_str.find("//archive") != std::string::npos) {
    return path_str;
  }
  if (fi.location().is_archive()) {
    return fi.location().as_url();
  }
  std::error_code ec;
  const auto abs = std::filesystem::absolute(fi.path(), ec);
  return ec ? path_str : abs.lexically_normal().string();
}

/// Union of tags present on any selected file (checksum-cache hits only; no hashing).
QStringList tags_on_selection(const std::vector<dirtoo::fs::FileInfo>& files)
{
  dirtoo::hash::ChecksumStore checksums;
  dirtoo::tags::TagStore tags;
  std::string err;
  if (!checksums.open(dirtoo::hash::ChecksumStore::default_path(), &err)
      || !tags.open(dirtoo::tags::TagStore::default_path(), &err)) {
    return {};
  }
  std::set<std::string> all;
  for (const auto& fi : files) {
    for (const auto& name : tags.tags_for_path(checksums, path_key_for(fi))) {
      all.insert(name);
    }
  }
  QStringList out;
  for (const auto& n : all) {
    out << QString::fromStdString(n);
  }
  return out;
}

/// Dialog: current tags (remove) + line edit / known list (add).
class TagNameDialog : public QDialog {
public:
  enum class Action { Cancel, Add, Remove };

  explicit TagNameDialog(int file_count, const QStringList& current_tags,
                         QWidget* parent = nullptr)
      : QDialog(parent)
  {
    setWindowTitle(QStringLiteral("Tag"));
    auto* layout = new QVBoxLayout(this);

    // --- Current tags (remove) ---
    layout->addWidget(new QLabel(
        QStringLiteral("Current tags on the selection (%1 file(s)).\n"
                       "Select one or more, then Remove.")
            .arg(file_count),
        this));
    current_list_ = new QListWidget(this);
    current_list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    current_list_->addItems(current_tags);
    current_list_->setMinimumHeight(100);
    current_list_->setEnabled(!current_tags.isEmpty());
    layout->addWidget(current_list_);

    remove_btn_ = new QPushButton(QStringLiteral("Remove selected"), this);
    remove_btn_->setEnabled(!current_tags.isEmpty());
    connect(remove_btn_, &QPushButton::clicked, this, [this] {
      action_ = Action::Remove;
      accept();
    });
    connect(current_list_, &QListWidget::itemSelectionChanged, this, [this] {
      remove_btn_->setEnabled(!current_list_->selectedItems().isEmpty());
    });
    // Double-click removes that single tag immediately.
    connect(current_list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
      if (item == nullptr) {
        return;
      }
      current_list_->clearSelection();
      item->setSelected(true);
      action_ = Action::Remove;
      accept();
    });
    layout->addWidget(remove_btn_);

    // --- Add tags ---
    layout->addWidget(new QLabel(
        QStringLiteral("Add tag name(s).\n"
                       "Separate multiple tags with spaces or commas.\n"
                       "Click a known tag below to append it."),
        this));

    edit_ = new QLineEdit(this);
    edit_->setPlaceholderText(QStringLiteral("e.g. work, game:doom location-paris"));
    layout->addWidget(edit_);

    known_ = known_tag_names();
    auto* model = new QStringListModel(known_, this);
    auto* completer = new QCompleter(model, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    edit_->setCompleter(completer);

    known_list_ = new QListWidget(this);
    known_list_->addItems(known_);
    known_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    known_list_->setMinimumHeight(120);
    layout->addWidget(known_list_);
    connect(known_list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
      if (item == nullptr) {
        return;
      }
      QString text = edit_->text().trimmed();
      const QString name = item->text();
      if (text.isEmpty()) {
        edit_->setText(name);
      } else {
        const QStringList parts = split_tag_names(text);
        if (!parts.contains(name)) {
          edit_->setText(text + QLatin1Char(' ') + name);
        }
      }
      edit_->setFocus();
      edit_->setCursorPosition(edit_->text().size());
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Add"));
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
      action_ = Action::Add;
      accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, [this] {
      action_ = Action::Cancel;
      reject();
    });
    layout->addWidget(buttons);

    edit_->setFocus();
    resize(440, 480);
  }

  [[nodiscard]] Action action() const { return action_; }

  [[nodiscard]] QStringList tags_to_add() const { return split_tag_names(edit_->text()); }

  [[nodiscard]] QStringList tags_to_remove() const
  {
    QStringList out;
    for (QListWidgetItem* item : current_list_->selectedItems()) {
      if (item != nullptr && !item->text().isEmpty()) {
        out << item->text();
      }
    }
    return out;
  }

private:
  QLineEdit* edit_ = nullptr;
  QListWidget* known_list_ = nullptr;
  QListWidget* current_list_ = nullptr;
  QPushButton* remove_btn_ = nullptr;
  QStringList known_;
  Action action_ = Action::Cancel;
};

} // namespace

TagController::TagController(QObject* parent)
    : QObject(parent)
{
}

void TagController::set_dialog_parent(QWidget* parent)
{
  dialog_parent_ = parent;
}

void TagController::tag_files(std::vector<dirtoo::fs::FileInfo> selection)
{
  std::vector<dirtoo::fs::FileInfo> files;
  files.reserve(selection.size());
  for (auto& fi : selection) {
    // Regular files only (directories are not tagged). Search hits and tag://
    // listings are synthetic FileInfos but still have a real path and
    // is_regular_file() — do not reject those (previously required non-synthetic
    // or archive, which blocked Tag… from recursive search results).
    if (!fi.is_regular_file()) {
      continue;
    }
    if (fi.path().empty() && !fi.location().is_archive()) {
      continue;
    }
    files.push_back(std::move(fi));
  }
  if (files.empty()) {
    QMessageBox::information(dialog_parent_, QStringLiteral("Tag"),
                             QStringLiteral("Select one or more regular files first "
                                            "(disk files, search hits, or archive members)."));
    return;
  }

  const QStringList current = tags_on_selection(files);
  TagNameDialog dlg(static_cast<int>(files.size()), current, dialog_parent_);
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  const TagJob::Mode mode =
      (dlg.action() == TagNameDialog::Action::Remove) ? TagJob::Mode::Remove : TagJob::Mode::Add;
  const QStringList tag_list =
      (mode == TagJob::Mode::Remove) ? dlg.tags_to_remove() : dlg.tags_to_add();
  if (tag_list.isEmpty()) {
    return;
  }

  Pending pending;
  pending.files = std::move(files);
  pending.tags = tag_list;
  pending.mode = mode;
  enqueue_job(std::move(pending));
}

void TagController::enqueue_job(Pending pending)
{
  const int n = static_cast<int>(pending.files.size());
  queue_.push_back(std::move(pending));
  if (active_job_ != nullptr || queue_.size() > 1) {
    emit status_message(
        QStringLiteral("Tag job queued (%1 file(s); %2 waiting)…")
            .arg(n)
            .arg(queue_.size() + (active_job_ != nullptr ? 1 : 0) - 1),
        3000);
  }
  start_next_job();
}

void TagController::start_next_job()
{
  if (active_job_ != nullptr || queue_.empty()) {
    return;
  }
  Pending pending = std::move(queue_.front());
  queue_.pop_front();

  const int total = static_cast<int>(pending.files.size());
  const QString tag_label = pending.tags.join(QLatin1Char(','));
  const QString activity_verb =
      (pending.mode == TagJob::Mode::Remove) ? QStringLiteral("Untagging")
                                             : QStringLiteral("Tagging");
  QString activity0 =
      tag_label.isEmpty() ? activity_verb
                          : QStringLiteral("%1 %2").arg(activity_verb, tag_label);
  const QString task_id =
      ActivityMonitor::instance().begin_job(QStringLiteral("tag"), activity0, 0, total);

  auto* job = new TagJob(std::move(pending.files), pending.tags, pending.mode, this);
  active_job_ = job;
  const TagJob::Mode mode = pending.mode;

  connect(job, &TagJob::progress, this,
          [task_id, tag_label, activity_verb](int done, int total_n, const QString& name) {
            QString activity =
                tag_label.isEmpty() ? activity_verb
                                    : QStringLiteral("%1 %2").arg(activity_verb, tag_label);
            if (!name.isEmpty()) {
              activity += QStringLiteral(" — %1").arg(name);
            }
            ActivityMonitor::instance().update_job(task_id, activity, done, total_n);
          });
  connect(job, &TagJob::failed, this, [this, job, task_id](const QString& message) {
    if (active_job_ == job) {
      active_job_ = nullptr;
    }
    ActivityMonitor::instance().end_job(task_id);
    QMessageBox::warning(dialog_parent_, QStringLiteral("Tag"), message);
    job->deleteLater();
    start_next_job();
  });
  connect(job, &TagJob::finished, this,
          [this, job, mode, task_id](int changed, int skipped, const QStringList& problems) {
            if (active_job_ == job) {
              active_job_ = nullptr;
            }
            const QString done_verb =
                (mode == TagJob::Mode::Remove) ? QStringLiteral("Removed tags from")
                                               : QStringLiteral("Tagged");
            QString msg = QStringLiteral("%1 %2 file(s).").arg(done_verb).arg(changed);
            if (skipped > 0) {
              msg += QStringLiteral(" Skipped %1.").arg(skipped);
            }
            if (!problems.isEmpty() && problems.size() <= 5) {
              msg += QLatin1Char('\n') + problems.join(QLatin1Char('\n'));
            }
            ActivityMonitor::instance().end_job(task_id);
            emit status_message(msg, 5000);
            if (changed > 0) {
              emit tags_applied(changed);
            }
            if (skipped > 0 && !problems.isEmpty()) {
              QMessageBox::warning(dialog_parent_, QStringLiteral("Tag"), msg);
            }
            job->deleteLater();
            start_next_job();
          });

  const QString activity =
      tag_label.isEmpty() ? activity_verb
                          : QStringLiteral("%1 %2").arg(activity_verb, tag_label);
  // task registered via begin_job above
  emit status_message(QStringLiteral("%1 (%2 file(s))…").arg(activity).arg(total), 3000);
  job->start();
}

} // namespace dirtoo::app

