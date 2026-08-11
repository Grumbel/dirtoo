// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "checksum_dialog.hpp"

#include "dirtoo/hash/checksum_store.hpp"
#include "dirtoo/hash/hash_file.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <atomic>
#include <utility>

namespace dirtoo::app {
namespace {

struct RowResult {
  QString path;
  QString status; // Cached / Hashed / Error / Missing
  QString crc32;
  QString md5;
  QString sha1;
  QString sha256;
  QString error;
};

class ChecksumWorker : public QObject {
  Q_OBJECT
public:
  explicit ChecksumWorker(QStringList paths, bool refresh, bool cached_only)
      : paths_(std::move(paths))
      , refresh_(refresh)
      , cached_only_(cached_only)
  {
  }

public slots:
  void run()
  {
    dirtoo::hash::ChecksumStore store;
    std::string err;
    if (!store.open(dirtoo::hash::ChecksumStore::default_path(), &err)) {
      emit finished_all();
      emit failed(QString::fromStdString(err));
      return;
    }

    const int total = paths_.size();
    for (int i = 0; i < total; ++i) {
      if (cancel_.load()) {
        break;
      }
      emit progress(i, total);

      RowResult row;
      row.path = paths_[i];
      const QFileInfo fi(row.path);
      if (!fi.exists() || !fi.isFile()) {
        row.status = QStringLiteral("Skip");
        row.error = QStringLiteral("not a regular file");
        emit row_ready(row.path, row.status, row.crc32, row.md5, row.sha1, row.sha256, row.error);
        continue;
      }

      std::error_code ec;
      const auto abs = std::filesystem::absolute(row.path.toStdString(), ec);
      const std::string key = ec ? row.path.toStdString() : abs.lexically_normal().string();

      if (cached_only_) {
        if (auto d = store.get(key)) {
          row.status = QStringLiteral("Cached");
          row.crc32 = QString::fromStdString(d->crc32_hex);
          row.md5 = QString::fromStdString(d->md5_hex);
          row.sha1 = QString::fromStdString(d->sha1_hex);
          row.sha256 = QString::fromStdString(d->sha256_hex);
        } else {
          row.status = QStringLiteral("Missing");
          row.error = QStringLiteral("not in cache");
        }
        emit row_ready(row.path, row.status, row.crc32, row.md5, row.sha1, row.sha256, row.error);
        continue;
      }

      dirtoo::hash::HashError herr;
      if (auto d = store.ensure(abs, key, refresh_, &herr)) {
        row.status = refresh_ ? QStringLiteral("Hashed") : QStringLiteral("OK");
        row.crc32 = QString::fromStdString(d->crc32_hex);
        row.md5 = QString::fromStdString(d->md5_hex);
        row.sha1 = QString::fromStdString(d->sha1_hex);
        row.sha256 = QString::fromStdString(d->sha256_hex);
      } else {
        row.status = QStringLiteral("Error");
        row.error = QString::fromStdString(herr.message);
      }
      emit row_ready(row.path, row.status, row.crc32, row.md5, row.sha1, row.sha256, row.error);
    }
    emit progress(total, total);
    emit finished_all();
  }

  void request_cancel() { cancel_.store(true); }

signals:
  void progress(int done, int total);
  void row_ready(const QString& path, const QString& status, const QString& crc32,
                 const QString& md5, const QString& sha1, const QString& sha256,
                 const QString& error);
  void finished_all();
  void failed(const QString& message);

private:
  QStringList paths_;
  bool refresh_ = false;
  bool cached_only_ = false;
  std::atomic<bool> cancel_{false};
};

void fill_item(QTreeWidgetItem* item, const RowResult& row)
{
  item->setText(0, row.path);
  item->setText(1, row.status);
  item->setText(2, row.crc32);
  item->setText(3, row.md5);
  item->setText(4, row.sha1);
  item->setText(5, row.sha256);
  if (!row.error.isEmpty()) {
    item->setToolTip(1, row.error);
  }
  item->setData(5, Qt::UserRole, row.sha256);
}

void run_dialog(QWidget* parent, QStringList paths)
{
  paths.removeDuplicates();
  paths.erase(std::remove_if(paths.begin(), paths.end(),
                             [](const QString& p) { return p.trimmed().isEmpty(); }),
              paths.end());

  auto* dialog = new QDialog(parent);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(QStringLiteral("Checksums"));
  dialog->resize(920, 420);

  auto* layout = new QVBoxLayout(dialog);
  auto* info = new QLabel(
      QStringLiteral("Cache: %1")
          .arg(QString::fromStdString(dirtoo::hash::ChecksumStore::default_path().string())),
      dialog);
  info->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(info);

  auto* tree = new QTreeWidget(dialog);
  tree->setColumnCount(6);
  tree->setHeaderLabels({QStringLiteral("Path"), QStringLiteral("Status"), QStringLiteral("CRC32"),
                         QStringLiteral("MD5"), QStringLiteral("SHA-1"), QStringLiteral("SHA-256")});
  tree->setRootIsDecorated(false);
  tree->setAlternatingRowColors(true);
  tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  tree->setUniformRowHeights(true);
  tree->header()->setStretchLastSection(true);
  tree->header()->resizeSection(0, 280);
  tree->header()->resizeSection(1, 70);
  layout->addWidget(tree, 1);

  auto* progress = new QProgressBar(dialog);
  progress->setRange(0, std::max(1, paths.size()));
  progress->setValue(0);
  layout->addWidget(progress);

  auto* btn_row = new QHBoxLayout();
  auto* compute_btn = new QPushButton(QStringLiteral("Compute / Refresh"), dialog);
  auto* cached_btn = new QPushButton(QStringLiteral("Show cached only"), dialog);
  auto* copy_btn = new QPushButton(QStringLiteral("Copy SHA-256"), dialog);
  auto* clear_btn = new QPushButton(QStringLiteral("Clear cache entries"), dialog);
  auto* cancel_btn = new QPushButton(QStringLiteral("Cancel"), dialog);
  cancel_btn->setEnabled(false);
  btn_row->addWidget(compute_btn);
  btn_row->addWidget(cached_btn);
  btn_row->addWidget(copy_btn);
  btn_row->addWidget(clear_btn);
  btn_row->addStretch(1);
  btn_row->addWidget(cancel_btn);
  layout->addLayout(btn_row);

  auto* close_box = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
  layout->addWidget(close_box);
  QObject::connect(close_box, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

  // Seed rows
  for (const QString& p : paths) {
    auto* item = new QTreeWidgetItem(tree);
    item->setText(0, p);
    item->setText(1, QStringLiteral("…"));
  }

  QThread* thread = nullptr;
  ChecksumWorker* worker = nullptr;

  const auto stop_worker = [&] {
    if (worker != nullptr) {
      worker->request_cancel();
    }
    if (thread != nullptr) {
      thread->quit();
      thread->wait(3000);
      thread = nullptr;
      worker = nullptr;
    }
    cancel_btn->setEnabled(false);
    compute_btn->setEnabled(true);
    cached_btn->setEnabled(true);
  };

  const auto start_job = [&](bool refresh, bool cached_only) {
    stop_worker();
    tree->clear();
    for (const QString& p : paths) {
      auto* item = new QTreeWidgetItem(tree);
      item->setText(0, p);
      item->setText(1, QStringLiteral("…"));
    }
    progress->setValue(0);
    progress->setMaximum(std::max(1, paths.size()));
    compute_btn->setEnabled(false);
    cached_btn->setEnabled(false);
    cancel_btn->setEnabled(true);

    thread = new QThread(dialog);
    worker = new ChecksumWorker(paths, refresh, cached_only);
    worker->moveToThread(thread);
    QObject::connect(thread, &QThread::started, worker, &ChecksumWorker::run);
    QObject::connect(worker, &ChecksumWorker::progress, dialog,
                     [progress](int done, int total) {
                       progress->setMaximum(std::max(1, total));
                       progress->setValue(done);
                     });
    QObject::connect(
        worker, &ChecksumWorker::row_ready, dialog,
        [tree](const QString& path, const QString& status, const QString& crc32, const QString& md5,
               const QString& sha1, const QString& sha256, const QString& error) {
          RowResult row;
          row.path = path;
          row.status = status;
          row.crc32 = crc32;
          row.md5 = md5;
          row.sha1 = sha1;
          row.sha256 = sha256;
          row.error = error;
          for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            auto* item = tree->topLevelItem(i);
            if (item->text(0) == row.path) {
              fill_item(item, row);
              return;
            }
          }
          auto* item = new QTreeWidgetItem(tree);
          fill_item(item, row);
        });
    QObject::connect(worker, &ChecksumWorker::failed, dialog, [dialog](const QString& msg) {
      QMessageBox::warning(dialog, QStringLiteral("Checksums"), msg);
    });
    QObject::connect(worker, &ChecksumWorker::finished_all, dialog, [=, &thread, &worker]() {
      cancel_btn->setEnabled(false);
      compute_btn->setEnabled(true);
      cached_btn->setEnabled(true);
      if (thread != nullptr) {
        thread->quit();
      }
    });
    QObject::connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    QObject::connect(thread, &QThread::finished, dialog, [&thread, &worker] {
      thread = nullptr;
      worker = nullptr;
    });
    thread->start();
  };

  QObject::connect(compute_btn, &QPushButton::clicked, dialog, [=] { start_job(true, false); });
  QObject::connect(cached_btn, &QPushButton::clicked, dialog, [=] { start_job(false, true); });
  QObject::connect(cancel_btn, &QPushButton::clicked, dialog, [=] {
    if (worker != nullptr) {
      worker->request_cancel();
    }
  });
  QObject::connect(copy_btn, &QPushButton::clicked, dialog, [tree] {
    QStringList lines;
    const auto items = tree->selectedItems();
    const auto use = items.isEmpty() ? [&] {
      QList<QTreeWidgetItem*> all;
      for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        all.push_back(tree->topLevelItem(i));
      }
      return all;
    }()
                                     : items;
    for (QTreeWidgetItem* item : use) {
      const QString sha = item->data(5, Qt::UserRole).toString();
      if (!sha.isEmpty()) {
        lines << QStringLiteral("%1  %2").arg(sha, item->text(0));
      }
    }
    if (!lines.isEmpty()) {
      QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    }
  });
  QObject::connect(clear_btn, &QPushButton::clicked, dialog, [tree, dialog] {
    dirtoo::hash::ChecksumStore store;
    std::string err;
    if (!store.open(dirtoo::hash::ChecksumStore::default_path(), &err)) {
      QMessageBox::warning(dialog, QStringLiteral("Checksums"), QString::fromStdString(err));
      return;
    }
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
      auto* item = tree->topLevelItem(i);
      std::error_code ec;
      const auto abs = std::filesystem::absolute(item->text(0).toStdString(), ec);
      const std::string key = ec ? item->text(0).toStdString() : abs.lexically_normal().string();
      store.remove(key);
      item->setText(1, QStringLiteral("Cleared"));
      item->setText(2, {});
      item->setText(3, {});
      item->setText(4, {});
      item->setText(5, {});
      item->setData(5, Qt::UserRole, {});
    }
  });

  QObject::connect(dialog, &QDialog::finished, dialog, [=](int) {
    if (worker != nullptr) {
      worker->request_cancel();
    }
    if (thread != nullptr) {
      thread->quit();
      thread->wait(2000);
    }
  });

  // Default: try cache first (fast), user can Refresh to force hash.
  start_job(false, false);
  dialog->show();
}

} // namespace

void show_checksum_dialog(QWidget* parent, const std::vector<std::filesystem::path>& paths)
{
  QStringList list;
  list.reserve(static_cast<int>(paths.size()));
  for (const auto& p : paths) {
    list << QString::fromStdString(p.string());
  }
  show_checksum_dialog(parent, list);
}

void show_checksum_dialog(QWidget* parent, const QStringList& paths)
{
  run_dialog(parent, paths);
}

} // namespace dirtoo::app

#include "checksum_dialog.moc"
