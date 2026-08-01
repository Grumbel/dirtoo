// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "about_dialog.hpp"

#include <QApplication>
#include <QMessageBox>

namespace dirtoo::app {

void show_about_dialog(QWidget* parent)
{
  const QString version = QApplication::applicationVersion();
  QMessageBox::about(
      parent, QStringLiteral("About dirtoo"),
      QStringLiteral(
          "<h3>dirtoo %1</h3>"
          "<p>Modular Qt file manager (C++23).</p>"
          "<p>Filesystem mutations are handled by the <b>dirops</b> library "
          "(copy, move, rename, delete).</p>"
          "<p>Archive browsing is <b>read-only</b> (table-of-contents listing "
          "with on-demand member extract).</p>"
          "<p>License: <b>GPL-3.0-or-later</b></p>"
          "<p>Copyright 2026 Ingo Ruhnke &lt;grumbel@gmail.com&gt;</p>")
          .arg(version));
}

} // namespace dirtoo::app
