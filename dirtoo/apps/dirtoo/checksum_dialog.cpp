// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "checksum_dialog.hpp"
#include "activity_monitor.hpp"

#include "dirtoo/hash/checksum_store.hpp"
#include "dirtoo/hash/hash_file.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QAction>
#include <QGuiApplication>
#include <QMenu>
#include <QClipboard>
#include <QDialogButtonBox>
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

class ChecksumWorker : public QObject {
  Q_OBJECT
public:
  ChecksumWorker(QStringList paths, bool refresh, bool cached_only, bool quick = false)
      : paths_(std::move(paths))
      , refresh_(refresh)
      , cached_only_(cached_only)
      , quick_(quick)
  {
  }

public slots:
  void run()
  {
    dirtoo::hash::ChecksumStore store;
    std::string err;
    if (!store.open(dirtoo::hash::ChecksumStore::default_path(), &err)) {
      emit failed(QString::fromStdString(err));
      emit finished_all();
      return;
    }

    const int total = static_cast<int>(paths_.size());
    for (int i = 0; i < total; ++i) {
      if (cancel_.load()) {
        break;
      }
      emit progress(i, total);

      const QString path = paths_[i];
      QString status;
      QString crc32;
      QString md5;
      QString sha1;
      QString sha256;
      QString error;

      const QFileInfo fi(path);
      if (!fi.exists() || !fi.isFile()) {
        status = QStringLiteral("Skip");
        error = QStringLiteral("not a regular file");
        emit row_ready(path, status, crc32, md5, sha1, sha256, error);
        continue;
      }

      std::error_code ec;
      const auto abs = std::filesystem::absolute(path.toStdString(), ec);
      const std::string key = ec ? path.toStdString() : abs.lexically_normal().string();

      if (cached_only_) {
        if (auto d = store.get(key)) {
          status = QStringLiteral("Cached");
          crc32 = QString::fromStdString(d->crc32_hex);
          md5 = QString::fromStdString(d->md5_hex);
          sha1 = QString::fromStdString(d->sha1_hex);
          sha256 = QString::fromStdString(d->sha256_hex);
        } else if (auto d = store.get_quick(key)) {
          status = QStringLiteral("Quick cached");
          sha256 = QString::fromStdString(d->sha256_hex);
          error = QStringLiteral("sample only");
        } else {
          status = QStringLiteral("Missing");
          error = QStringLiteral("not in cache");
        }
        emit row_ready(path, status, crc32, md5, sha1, sha256, error);
        continue;
      }

      dirtoo::hash::HashError herr;
      if (quick_) {
        // Sample hash — stored under quick: key so it never collides with full
        // SHA-256 used for tags (ChecksumStore::ensure / TagJob).
        dirtoo::hash::QuickHashOptions qopts;
        qopts.should_cancel = [this] { return cancel_.load(); };
        if (auto d = dirtoo::hash::hash_file_quick(abs, qopts, &herr)) {
          store.put_quick(key, *d);
          status = QStringLiteral("Quick");
          sha256 = QString::fromStdString(d->sha256_hex);
          error = QStringLiteral("sample (head/mid/tail); stored as quick:");
        } else {
          status = QStringLiteral("Error");
          error = QString::fromStdString(herr.message);
        }
      } else if (auto d = store.ensure(abs, key, refresh_, &herr)) {
        status = refresh_ ? QStringLiteral("Hashed") : QStringLiteral("OK");
        crc32 = QString::fromStdString(d->crc32_hex);
        md5 = QString::fromStdString(d->md5_hex);
        sha1 = QString::fromStdString(d->sha1_hex);
        sha256 = QString::fromStdString(d->sha256_hex);
      } else {
        status = QStringLiteral("Error");
        error = QString::fromStdString(herr.message);
      }
      emit row_ready(path, status, crc32, md5, sha1, sha256, error);
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
  bool quick_ = false;
  std::atomic<bool> cancel_{false};
};

} // namespace

ChecksumDialog::ChecksumDialog(QStringList paths, QWidget* parent)
    : QDialog(parent)
    , paths_(std::move(paths))
{
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowTitle(QStringLiteral("Checksums"));
  resize(920, 420);

  paths_.removeDuplicates();
  paths_.erase(std::remove_if(paths_.begin(), paths_.end(),
                              [](const QString& p) { return p.trimmed().isEmpty(); }),
               paths_.end());

  auto* layout = new QVBoxLayout(this);
  auto* info = new QLabel(
      QStringLiteral("Cache: %1")
          .arg(QString::fromStdString(dirtoo::hash::ChecksumStore::default_path().string())),
      this);
  info->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(info);

  tree_ = new QTreeWidget(this);
  tree_->setColumnCount(6);
  tree_->setHeaderLabels({QStringLiteral("Path"), QStringLiteral("Status"), QStringLiteral("CRC32"),
                          QStringLiteral("MD5"), QStringLiteral("SHA-1"), QStringLiteral("SHA-256")});
  tree_->setRootIsDecorated(false);
  tree_->setAlternatingRowColors(true);
  tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  tree_->setUniformRowHeights(true);
  tree_->setContextMenuPolicy(Qt::CustomContextMenu);
  tree_->header()->setStretchLastSection(true);
  tree_->header()->resizeSection(0, 280);
  tree_->header()->resizeSection(1, 70);
  layout->addWidget(tree_, 1);
  connect(tree_, &QTreeWidget::customContextMenuRequested, this,
          &ChecksumDialog::show_tree_context_menu);

  progress_ = new QProgressBar(this);
  progress_->setRange(0, std::max(qsizetype{1}, paths_.size()));
  progress_->setValue(0);
  layout->addWidget(progress_);

  auto* btn_row = new QHBoxLayout();
  compute_btn_ = new QPushButton(QStringLiteral("Compute / Refresh"), this);
  cached_btn_ = new QPushButton(QStringLiteral("Show cached only"), this);
  quick_btn_ = new QPushButton(QStringLiteral("Quick sample"), this);
  quick_btn_->setToolTip(
      QStringLiteral("Hash ~1 MiB from head, middle, and tail only. Fast on large files; "
                     "weaker than a full hash. Result is not written to the checksum cache "
                     "(tags still need a full SHA-256)."));
  auto* clear_btn = new QPushButton(QStringLiteral("Clear cache entries"), this);
  cancel_btn_ = new QPushButton(QStringLiteral("Cancel"), this);
  cancel_btn_->setEnabled(false);
  btn_row->addWidget(compute_btn_);
  btn_row->addWidget(cached_btn_);
  btn_row->addWidget(quick_btn_);
  btn_row->addWidget(clear_btn);
  btn_row->addStretch(1);
  btn_row->addWidget(cancel_btn_);
  layout->addLayout(btn_row);

  auto* close_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
  layout->addWidget(close_box);
  connect(close_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  for (const QString& p : paths_) {
    auto* item = new QTreeWidgetItem(tree_);
    item->setText(0, p);
    item->setText(1, QStringLiteral("…"));
  }

  connect(compute_btn_, &QPushButton::clicked, this, &ChecksumDialog::start_compute);
  connect(cached_btn_, &QPushButton::clicked, this, &ChecksumDialog::start_cached_only);
  connect(quick_btn_, &QPushButton::clicked, this, &ChecksumDialog::start_quick);
  connect(cancel_btn_, &QPushButton::clicked, this, &ChecksumDialog::cancel_job);
  connect(clear_btn, &QPushButton::clicked, this, &ChecksumDialog::clear_cache_entries);

  // Default: use cache, hash only on miss.
  start_job(false, false, false);
}

ChecksumDialog::~ChecksumDialog()
{
  stop_worker();
}

void ChecksumDialog::stop_worker()
{
  ActivityMonitor::instance().clear_task(QStringLiteral("checksum"));
  if (auto* w = qobject_cast<ChecksumWorker*>(worker_)) {
    w->request_cancel();
  }
  if (thread_ != nullptr) {
    thread_->quit();
    thread_->wait(3000);
    thread_ = nullptr;
    worker_ = nullptr;
  }
  if (cancel_btn_ != nullptr) {
    cancel_btn_->setEnabled(false);
  }
  if (compute_btn_ != nullptr) {
    compute_btn_->setEnabled(true);
  }
  if (cached_btn_ != nullptr) {
    cached_btn_->setEnabled(true);
  }
  if (quick_btn_ != nullptr) {
    quick_btn_->setEnabled(true);
  }
}

void ChecksumDialog::start_job(bool refresh, bool cached_only, bool quick)
{
  stop_worker();
  tree_->clear();
  for (const QString& p : paths_) {
    auto* item = new QTreeWidgetItem(tree_);
    item->setText(0, p);
    item->setText(1, QStringLiteral("…"));
  }
  progress_->setValue(0);
  progress_->setMaximum(std::max(qsizetype{1}, paths_.size()));
  compute_btn_->setEnabled(false);
  cached_btn_->setEnabled(false);
  if (quick_btn_ != nullptr) {
    quick_btn_->setEnabled(false);
  }
  cancel_btn_->setEnabled(true);

  thread_ = new QThread(this);
  auto* worker = new ChecksumWorker(paths_, refresh, cached_only, quick);
  worker_ = worker;
  worker->moveToThread(thread_);

  connect(thread_, &QThread::started, worker, &ChecksumWorker::run);
  connect(worker, &ChecksumWorker::progress, this, &ChecksumDialog::on_progress);
  connect(worker, &ChecksumWorker::progress, this, [](int done, int total) {
    ActivityMonitor::instance().set_task(QStringLiteral("checksum"), QStringLiteral("Checksums"),
                                         done, total);
  });
  connect(worker, &ChecksumWorker::row_ready, this, &ChecksumDialog::on_row);
  connect(worker, &ChecksumWorker::failed, this, &ChecksumDialog::on_failed);
  connect(worker, &ChecksumWorker::finished_all, this, &ChecksumDialog::on_finished);
  connect(thread_, &QThread::finished, worker, &QObject::deleteLater);
  connect(thread_, &QThread::finished, thread_, &QObject::deleteLater);
  connect(thread_, &QThread::finished, this, [this] {
    thread_ = nullptr;
    worker_ = nullptr;
  });
  thread_->start();
}

void ChecksumDialog::start_compute()
{
  start_job(true, false, false);
}

void ChecksumDialog::start_cached_only()
{
  start_job(false, true, false);
}

void ChecksumDialog::start_quick()
{
  start_job(false, false, true);
}

void ChecksumDialog::cancel_job()
{
  if (auto* w = qobject_cast<ChecksumWorker*>(worker_)) {
    w->request_cancel();
  }
  ActivityMonitor::instance().clear_task(QStringLiteral("checksum"));
}

void ChecksumDialog::on_progress(int done, int total)
{
  progress_->setMaximum(std::max(1, total));
  progress_->setValue(done);
}

void ChecksumDialog::on_row(const QString& path, const QString& status, const QString& crc32,
                            const QString& md5, const QString& sha1, const QString& sha256,
                            const QString& error)
{
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    auto* item = tree_->topLevelItem(i);
    if (item->text(0) == path) {
      item->setText(1, status);
      item->setText(2, crc32);
      item->setText(3, md5);
      item->setText(4, sha1);
      item->setText(5, sha256);
      item->setData(2, Qt::UserRole, crc32);
      item->setData(3, Qt::UserRole, md5);
      item->setData(4, Qt::UserRole, sha1);
      item->setData(5, Qt::UserRole, sha256);
      item->setToolTip(2, crc32.isEmpty() ? QString() : QStringLiteral("Right-click to copy CRC32"));
      item->setToolTip(3, md5.isEmpty() ? QString() : QStringLiteral("Right-click to copy MD5"));
      item->setToolTip(4, sha1.isEmpty() ? QString() : QStringLiteral("Right-click to copy SHA-1"));
      item->setToolTip(5, sha256.isEmpty() ? QString() : QStringLiteral("Right-click to copy SHA-256"));
      if (!error.isEmpty()) {
        item->setToolTip(1, error);
      }
      return;
    }
  }
  auto* item = new QTreeWidgetItem(tree_);
  item->setText(0, path);
  item->setText(1, status);
  item->setText(2, crc32);
  item->setText(3, md5);
  item->setText(4, sha1);
  item->setText(5, sha256);
  item->setData(2, Qt::UserRole, crc32);
  item->setData(3, Qt::UserRole, md5);
  item->setData(4, Qt::UserRole, sha1);
  item->setData(5, Qt::UserRole, sha256);
}

void ChecksumDialog::on_finished()
{
  ActivityMonitor::instance().clear_task(QStringLiteral("checksum"));
  cancel_btn_->setEnabled(false);
  compute_btn_->setEnabled(true);
  cached_btn_->setEnabled(true);
  if (quick_btn_ != nullptr) {
    quick_btn_->setEnabled(true);
  }
  if (thread_ != nullptr) {
    thread_->quit();
  }
}

void ChecksumDialog::on_failed(const QString& message)
{
  QMessageBox::warning(this, QStringLiteral("Checksums"), message);
}

void ChecksumDialog::show_tree_context_menu(const QPoint& pos)
{
  QTreeWidgetItem* under = tree_->itemAt(pos);
  if (under != nullptr && !under->isSelected()) {
    tree_->setCurrentItem(under);
    under->setSelected(true);
  }

  auto selected = tree_->selectedItems();
  if (selected.isEmpty() && under != nullptr) {
    selected.push_back(under);
  }
  if (selected.isEmpty()) {
    return;
  }

  // Prefer the column under the cursor so right-click on MD5 copies MD5.
  int col = tree_->columnAt(pos.x());
  if (col < 2 || col > 5) {
    col = 5; // default to SHA-256
  }

  auto* menu = new QMenu(this);
  const struct {
    int column;
    const char* label;
  } kDigests[] = {
      {2, "CRC32"},
      {3, "MD5"},
      {4, "SHA-1"},
      {5, "SHA-256"},
  };
  for (const auto& d : kDigests) {
    auto* act = menu->addAction(QStringLiteral("Copy %1").arg(QString::fromUtf8(d.label)));
    if (d.column == col) {
      menu->setDefaultAction(act);
    }
    const int column = d.column;
    connect(act, &QAction::triggered, this, [this, column] { copy_digest_column(column, false); });
  }
  menu->addSeparator();
  auto* with_path = menu->addAction(QStringLiteral("Copy %1 with path")
                                        .arg(QString::fromUtf8(kDigests[col - 2].label)));
  connect(with_path, &QAction::triggered, this, [this, col] { copy_digest_column(col, true); });
  auto* all_digests = menu->addAction(QStringLiteral("Copy all digests (this selection)"));
  connect(all_digests, &QAction::triggered, this, [this] {
    QStringList blocks;
    auto items = tree_->selectedItems();
    if (items.isEmpty()) {
      return;
    }
    for (QTreeWidgetItem* item : items) {
      QStringList parts;
      parts << item->text(0);
      for (int c = 2; c <= 5; ++c) {
        const QString v = item->data(c, Qt::UserRole).toString();
        if (v.isEmpty()) {
          continue;
        }
        static const char* names[] = {"CRC32", "MD5", "SHA-1", "SHA-256"};
        parts << QStringLiteral("%1: %2").arg(QString::fromUtf8(names[c - 2]), v);
      }
      if (parts.size() > 1) {
        blocks << parts.join(QLatin1Char('\n'));
      }
    }
    if (!blocks.isEmpty()) {
      QApplication::clipboard()->setText(blocks.join(QStringLiteral("\n\n")));
    }
  });
  menu->addSeparator();
  auto* copy_path = menu->addAction(QStringLiteral("Copy path"));
  connect(copy_path, &QAction::triggered, this, [this] {
    QStringList paths;
    for (QTreeWidgetItem* item : tree_->selectedItems()) {
      paths << item->text(0);
    }
    if (!paths.isEmpty()) {
      QApplication::clipboard()->setText(paths.join(QLatin1Char('\n')));
    }
  });

  menu->exec(tree_->viewport()->mapToGlobal(pos));
  menu->deleteLater();
}

void ChecksumDialog::copy_digest_column(int column, bool include_path)
{
  if (column < 2 || column > 5 || tree_ == nullptr) {
    return;
  }
  QStringList lines;
  auto items = tree_->selectedItems();
  if (items.isEmpty()) {
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
      items.push_back(tree_->topLevelItem(i));
    }
  }
  for (QTreeWidgetItem* item : items) {
    QString dig = item->data(column, Qt::UserRole).toString();
    if (dig.isEmpty()) {
      dig = item->text(column).trimmed();
    }
    if (dig.isEmpty() || dig == QLatin1String("…")) {
      continue;
    }
    if (include_path) {
      lines << QStringLiteral("%1  %2").arg(dig, item->text(0));
    } else {
      lines << dig;
    }
  }
  if (!lines.isEmpty()) {
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
  }
}

void ChecksumDialog::clear_cache_entries()
{
  dirtoo::hash::ChecksumStore store;
  std::string err;
  if (!store.open(dirtoo::hash::ChecksumStore::default_path(), &err)) {
    QMessageBox::warning(this, QStringLiteral("Checksums"), QString::fromStdString(err));
    return;
  }
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    auto* item = tree_->topLevelItem(i);
    std::error_code ec;
    const auto abs = std::filesystem::absolute(item->text(0).toStdString(), ec);
    const std::string key = ec ? item->text(0).toStdString() : abs.lexically_normal().string();
    store.remove(key);
    item->setText(1, QStringLiteral("Cleared"));
    item->setText(2, {});
    item->setText(3, {});
    item->setText(4, {});
    item->setText(5, {});
    item->setData(2, Qt::UserRole, {});
    item->setData(3, Qt::UserRole, {});
    item->setData(4, Qt::UserRole, {});
    item->setData(5, Qt::UserRole, {});
  }
}

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
  auto* dialog = new ChecksumDialog(paths, parent);
  dialog->show();
}

} // namespace dirtoo::app

#include "checksum_dialog.moc"
