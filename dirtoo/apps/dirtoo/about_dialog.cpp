// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "about_dialog.hpp"
#include "badge_icons.hpp"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>

namespace dirtoo::app {

void show_about_dialog(QWidget* parent)
{
  QDialog dialog(parent);
  dialog.setWindowTitle(QStringLiteral("About dirtoo"));
  dialog.setMinimumWidth(400);

  auto* layout = new QVBoxLayout(&dialog);

  auto* icon = new QLabel(&dialog);
  icon->setAlignment(Qt::AlignHCenter);
  const QPixmap pm = load_badge_pixmap(QStringLiteral("dirtoo.png"));
  if (!pm.isNull()) {
    icon->setPixmap(pm.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
  layout->addWidget(icon);

  const QString version = QApplication::applicationVersion().isEmpty()
                              ? QStringLiteral("0.2.0-dev")
                              : QApplication::applicationVersion();

  auto* text = new QLabel(
      QStringLiteral(
          "<center>"
          "<h1>dirtoo</h1>"
          "<p>Version %1</p>"
          "<p>A modular Qt file viewer and manager (C++23).</p>"
          "<p>Filesystem mutations are handled by the <b>dirops</b> library "
          "(copy, move, rename, delete, mkdir).</p>"
          "<p>Archive browsing is <b>read-only</b> (TOC listing with "
          "on-demand member extract).</p>"
          "<p>Metadata uses an async SQLite cache; the GUI thread does not "
          "probe media or read the database.</p>"
          "<p><a href=\"https://github.com/grumbel/dirtoo\">Project page</a></p>"
          "<p>License: <b>GPL-3.0-or-later</b></p>"
          "<p>Copyright © 2017–2026 Ingo Ruhnke &lt;grumbel@gmail.com&gt;</p>"
          "</center>")
          .arg(version.toHtmlEscaped()),
      &dialog);
  text->setTextFormat(Qt::RichText);
  text->setOpenExternalLinks(true);
  text->setWordWrap(true);
  text->setAlignment(Qt::AlignHCenter);
  layout->addWidget(text);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  layout->addWidget(buttons);

  dialog.exec();
}

} // namespace dirtoo::app
