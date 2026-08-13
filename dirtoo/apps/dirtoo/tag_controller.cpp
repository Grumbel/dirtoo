// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tag_controller.hpp"

#include "tag_job.hpp"
#include "activity_monitor.hpp"

#include "dirtoo/tags/tag_store.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QCompleter>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStringListModel>
#include <QVBoxLayout>

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

/// Dialog: line edit (multi tag, completer) + clickable list of existing tags.
class TagNameDialog : public QDialog {
public:
  explicit TagNameDialog(int file_count, QWidget* parent = nullptr)
      : QDialog(parent)
  {
    setWindowTitle(QStringLiteral("Tag"));
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        QStringLiteral("Tag name(s) for %1 file(s).\n"
                       "Separate multiple tags with spaces or commas.\n"
                       "Click a known tag below to append it.")
            .arg(file_count),
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

    list_ = new QListWidget(this);
    list_->addItems(known_);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setMinimumHeight(160);
    layout->addWidget(list_);
    connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
      if (item == nullptr) {
        return;
      }
      QString text = edit_->text().trimmed();
      const QString name = item->text();
      if (text.isEmpty()) {
        edit_->setText(name);
      } else {
        // Avoid duplicates in the field.
        const QStringList parts = split_tag_names(text);
        if (!parts.contains(name)) {
          edit_->setText(text + QLatin1Char(' ') + name);
        }
      }
      edit_->setFocus();
      edit_->setCursorPosition(edit_->text().size());
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    // GNOME order via app style hint.
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    edit_->setFocus();
    resize(420, 360);
  }

  [[nodiscard]] QStringList tags() const { return split_tag_names(edit_->text()); }

private:
  QLineEdit* edit_ = nullptr;
  QListWidget* list_ = nullptr;
  QStringList known_;
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
    if (!fi.is_regular_file()) {
      continue;
    }
    if (fi.is_synthetic() && !fi.location().is_archive()) {
      continue;
    }
    files.push_back(std::move(fi));
  }
  if (files.empty()) {
    QMessageBox::information(dialog_parent_, QStringLiteral("Tag"),
                             QStringLiteral("Select one or more regular files first "
                                            "(disk files or archive members)."));
    return;
  }

  TagNameDialog dlg(static_cast<int>(files.size()), dialog_parent_);
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }
  const QStringList tag_list = dlg.tags();
  if (tag_list.isEmpty()) {
    return;
  }

  const int total = static_cast<int>(files.size());
  auto* progress = new QProgressDialog(QStringLiteral("Tagging files…"), QStringLiteral("Cancel"),
                                       0, total, dialog_parent_);
  progress->setWindowModality(Qt::WindowModal);
  progress->setMinimumDuration(total > 1 ? 0 : 2000);
  progress->setAttribute(Qt::WA_DeleteOnClose);
  progress->setValue(0);

  const QString tag_label = tag_list.join(QLatin1Char(','));

  // Hash + archive extract on a worker thread (AGENTS: no hash on GUI).
  auto* job = new TagJob(std::move(files), tag_list, this);
  connect(progress, &QProgressDialog::canceled, job, &TagJob::cancel);
  connect(job, &TagJob::progress, this,
          [progress, tag_label](int done, int total_n, const QString& name) {
            if (progress == nullptr) {
              return;
            }
            progress->setMaximum(total_n);
            progress->setValue(done);
            if (!name.isEmpty()) {
              progress->setLabelText(QStringLiteral("Tagging %1…").arg(name));
            }
            const QString activity =
                tag_label.isEmpty() ? QStringLiteral("Tagging")
                                    : QStringLiteral("Tagging %1").arg(tag_label);
            ActivityMonitor::instance().set_task(QStringLiteral("tag"), activity, done, total_n);
          });
  connect(job, &TagJob::failed, this, [this, progress, job](const QString& message) {
    if (progress != nullptr) {
      progress->close();
    }
    ActivityMonitor::instance().clear_task(QStringLiteral("tag"));
    QMessageBox::warning(dialog_parent_, QStringLiteral("Tag"), message);
    job->deleteLater();
  });
  connect(job, &TagJob::finished, this,
          [this, progress, job](int tagged, int skipped, const QStringList& problems) {
            if (progress != nullptr) {
              progress->setValue(progress->maximum());
              progress->close();
            }
            QString msg = QStringLiteral("Tagged %1 file(s).").arg(tagged);
            if (skipped > 0) {
              msg += QStringLiteral(" Skipped %1.").arg(skipped);
            }
            if (!problems.isEmpty() && problems.size() <= 5) {
              msg += QLatin1Char('\n') + problems.join(QLatin1Char('\n'));
            }
            ActivityMonitor::instance().clear_task(QStringLiteral("tag"));
            emit status_message(msg, 5000);
            if (tagged > 0) {
              emit tags_applied(tagged);
            }
            if (skipped > 0) {
              QMessageBox::warning(dialog_parent_, QStringLiteral("Tag"), msg);
            }
            job->deleteLater();
          });
  {
    const QString activity = tag_label.isEmpty() ? QStringLiteral("Tagging")
                                                 : QStringLiteral("Tagging %1").arg(tag_label);
    ActivityMonitor::instance().set_task(QStringLiteral("tag"), activity, 0, total);
  }
  job->start();
  progress->show();
}

} // namespace dirtoo::app
