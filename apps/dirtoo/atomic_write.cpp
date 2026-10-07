// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "atomic_write.hpp"

#include <QDebug>
#include <QSaveFile>
#include <QString>

#include <system_error>

namespace dirtoo::app {

bool write_file_atomic(const std::filesystem::path& path, std::string_view content,
                       std::string* error)
{
  auto fail = [&](const QString& why) {
    const QString msg = QStringLiteral("cannot write %1: %2")
                            .arg(QString::fromStdString(path.string()), why);
    qWarning().noquote() << "dirtoo:" << msg;
    if (error != nullptr) {
      *error = msg.toStdString();
    }
    return false;
  };

  if (auto parent = path.parent_path(); !parent.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      return fail(QString::fromStdString(ec.message()));
    }
  }

  QSaveFile file(QString::fromStdString(path.string()));
  file.setDirectWriteFallback(false);  // never degrade to truncate-in-place
  if (!file.open(QIODevice::WriteOnly)) {
    return fail(file.errorString());
  }
  if (file.write(content.data(), static_cast<qint64>(content.size()))
      != static_cast<qint64>(content.size())) {
    file.cancelWriting();
    return fail(file.errorString());
  }
  if (!file.commit()) {  // flush + rename; keeps the old file on failure
    return fail(file.errorString());
  }
  return true;
}

} // namespace dirtoo::app
