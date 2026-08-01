// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "about_dialog.hpp"

#include <QApplication>
#include <QMessageBox>

namespace dirtoo::app {

void show_about_dialog(QWidget* parent)
{
  QMessageBox::about(
      parent, QStringLiteral("About dirtoo"),
      QStringLiteral(
          "<h3>dirtoo %1</h3>"
          "<p>Modular Qt file manager (C++23 port).</p>"
          "<p>Filesystem operations are provided by the <b>dirops</b> library.</p>"
          "<p>License: GPL-3.0-or-later</p>"
          "<p>Copyright 2026 Ingo Ruhnke &lt;grumbel@gmail.com&gt;</p>")
          .arg(QApplication::applicationVersion()));
}

} // namespace dirtoo::app
