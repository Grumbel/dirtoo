# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2018 Ingo Ruhnke <grumbel@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


from typing import cast, Optional

import os

from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import (QWidget, QDialog, QPushButton, QLineEdit,
                             QHBoxLayout, QVBoxLayout,
                             QDialogButtonBox, QLabel)

from dirtoo.image.icon import load_icon


class RenameDialog(QDialog):

    def __init__(self, parent: Optional[QWidget]) -> None:
        super().__init__(parent)

        self._basename: str = ""

        self._build_gui()

    def _build_gui(self) -> None:
        self.resize(600, 100)
        self.setWindowTitle("RenameDialog")
        self.setWindowModality(Qt.WindowModality.WindowModal)

        # Widgets
        icon_label = QLabel()
        icon_label.setPixmap(load_icon("accessories-text-editor").pixmap(48))
        icon_label.setAlignment(Qt.AlignmentFlag.AlignTop)

        self.label = QLabel("Rename ...")
        self.label.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        self.name_edit = QLineEdit(self)

        self.name_reload = self.name_edit.addAction(load_icon("reload"), QLineEdit.ActionPosition.TrailingPosition)
        self.name_reload.setShortcut("F5")
        self.name_reload.setToolTip("Reset the filename to it's original name")

        self.button_box = QDialogButtonBox(self)

        self.btn_rename = QPushButton(load_icon("document-save-as"), "Rename")
        self.button_box.addButton(self.btn_rename, QDialogButtonBox.ButtonRole.AcceptRole)
        self.btn_cancel = self.button_box.addButton(QDialogButtonBox.StandardButton.Cancel)
        self.btn_rename.setDefault(True)

        # layout
        self.vbox = QVBoxLayout()

        self.vbox.addWidget(self.label)
        self.vbox.addWidget(self.name_edit)
        self.vbox.addStretch()
        self.vbox.addWidget(self.button_box)

        hbox = QHBoxLayout()
        hbox.addWidget(icon_label)
        hbox.addLayout(self.vbox)
        self.setLayout(hbox)

        # signals
        self.name_edit.returnPressed.connect(self.accept)
        self.btn_rename.clicked.connect(self._on_rename_clicked)
        self.btn_cancel.clicked.connect(self._on_cancel_clicked)
        self.name_reload.triggered.connect(self._on_name_reload)

    def _on_rename_clicked(self) -> None:
        self.accept()

    def _on_cancel_clicked(self) -> None:
        self.reject()

    def _on_name_reload(self) -> None:
        self.name_edit.setText(self._basename)
        root, ext = os.path.splitext(self._basename)
        self.name_edit.setSelection(0, len(root))

    def get_old_basename(self) -> str:
        return self._basename

    def get_new_basename(self) -> str:
        return cast(str, self.name_edit.text())

    def set_basename(self, basename: str) -> None:
        self._basename = basename

        self.setWindowTitle("Rename \"{}\"".format(basename))
        self.label.setText("Rename \"{}\" to:".format(basename))

        self.name_edit.setText(basename)
        root, ext = os.path.splitext(basename)
        self.name_edit.setSelection(0, len(root))


# EOF #
