// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "checksum_dialog.hpp"

#include "dirtoo/hash/checksum_store.hpp"
#include "dirtoo/hash/hash_file.hpp"

#include <QAbstractItemView>
#include <QApplication>
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
  ChecksumWorker(QStringList paths, bool refresh, bool cached_only)
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
        } else {
          status = QStringLiteral("Missing");
          error = QStringLiteral("not in cache");
        }
        emit row_ready(path, status, crc32, md5, sha1, sha256, error);
        continue;
      }

      dirtoo::hash::HashError herr;
      if (auto d = store.ensure(abs, key, refresh_, &herr)) {
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
  tree_->header()->setStretchLastSection(true);
  tree_->header()->resizeSection(0, 280);
  tree_->header()->resizeSection(1, 70);
  layout->addWidget(tree_, 1);

  progress_ = new QProgressBar(this);
  progress_->setRange(0, std::max(qsizetype{1}, paths_.size()));
  progress_->setValue(0);
  layout->addWidget(progress_);

  auto* btn_row = new QHBoxLayout();
  compute_btn_ = new QPushButton(QStringLiteral("Compute / Refresh"), this);
  cached_btn_ = new QPushButton(QStringLiteral("Show cached only"), this);
  auto* copy_btn = new QPushButton(QStringLiteral("Copy SHA-256"), this);
  auto* clear_btn = new QPushButton(QStringLiteral("Clear cache entries"), this);
  cancel_btn_ = new QPushButton(QStringLiteral("Cancel"), this);
  cancel_btn_->setEnabled(false);
  btn_row->addWidget(compute_btn_);
  btn_row->addWidget(cached_btn_);
  btn_row->addWidget(copy_btn);
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
  connect(cancel_btn_, &QPushButton::clicked, this, &ChecksumDialog::cancel_job);
  connect(copy_btn, &QPushButton::clicked, this, &ChecksumDialog::copy_sha256);
  connect(clear_btn, &QPushButton::clicked, this, &ChecksumDialog::clear_cache_entries);

  // Default: use cache, hash only on miss.
  start_job(false, false);
}

ChecksumDialog::~ChecksumDialog()
{
  stop_worker();
}

void ChecksumDialog::stop_worker()
{
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
}

void ChecksumDialog::start_job(bool refresh, bool cached_only)
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
  cancel_btn_->setEnabled(true);

  thread_ = new QThread(this);
  auto* worker = new ChecksumWorker(paths_, refresh, cached_only);
  worker_ = worker;
  worker->moveToThread(thread_);

  connect(thread_, &QThread::started, worker, &ChecksumWorker::run);
  connect(worker, &ChecksumWorker::progress, this, &ChecksumDialog::on_progress);
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
  start_job(true, false);
}

void ChecksumDialog::start_cached_only()
{
  start_job(false, true);
}

void ChecksumDialog::cancel_job()
{
  if (auto* w = qobject_cast<ChecksumWorker*>(worker_)) {
    w->request_cancel();
  }
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
      item->setData(5, Qt::UserRole, sha256);
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
  item->setData(5, Qt::UserRole, sha256);
}

void ChecksumDialog::on_finished()
{
  cancel_btn_->setEnabled(false);
  compute_btn_->setEnabled(true);
  cached_btn_->setEnabled(true);
  if (thread_ != nullptr) {
    thread_->quit();
  }
}

void ChecksumDialog::on_failed(const QString& message)
{
  QMessageBox::warning(this, QStringLiteral("Checksums"), message);
}

void ChecksumDialog::copy_sha256()
{
  QStringList lines;
  auto items = tree_->selectedItems();
  if (items.isEmpty()) {
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
      items.push_back(tree_->topLevelItem(i));
    }
  }
  for (QTreeWidgetItem* item : items) {
    const QString sha = item->data(5, Qt::UserRole).toString();
    if (!sha.isEmpty()) {
      lines << QStringLiteral("%1  %2").arg(sha, item->text(0));
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
