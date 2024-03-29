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


from typing import Optional

from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import (QWidget, QDialog, QPushButton, QLineEdit,
                             QHBoxLayout, QVBoxLayout,
                             QDialogButtonBox, QLabel)

from dirtoo.image.icon import load_icon


class CreateDialog(QDialog):

    FOLDER = 0
    TEXTFILE = 1

    def __init__(self, kind: int, parent: Optional[QWidget]) -> None:
        super().__init__(parent)

        self._kind = kind
        self._build_gui()

    def _build_gui(self) -> None:
        self.resize(400, 100)

        self.setWindowModality(Qt.WindowModality.WindowModal)

        self.name_edit = QLineEdit(self)
        self.name_edit.textEdited.connect(self._on_text_edited)

        self.icon = QLabel()
        if self._kind == CreateDialog.FOLDER:
            self.setWindowTitle("Create a folder")
            self.label = QLabel("Enter the name for the folder:")
            self.icon.setPixmap(load_icon("folder-new").pixmap(48))
            self.name_edit.setText("New Folder")
            self.name_edit.selectAll()
        elif self._kind == CreateDialog.TEXTFILE:
            self.setWindowTitle("Create an empty file")
            self.label = QLabel("Enter the name for the file:")
            self.icon.setPixmap(load_icon("document-new").pixmap(48))
            self.name_edit.setText("New File")
            self.name_edit.selectAll()

        self.button_box = QDialogButtonBox(self)

        self.btn_create = QPushButton(load_icon("document-save-as"), "Create")
        self.button_box.addButton(self.btn_create, QDialogButtonBox.ButtonRole.AcceptRole)
        self.btn_cancel = self.button_box.addButton(QDialogButtonBox.StandardButton.Cancel)
        self.btn_create.setDefault(True)

        # layout
        self.subvbox = QVBoxLayout()
        self.subvbox.addWidget(self.label)
        self.subvbox.addWidget(self.name_edit)

        self.hbox = QHBoxLayout()
        self.hbox.addWidget(self.icon)
        self.hbox.addLayout(self.subvbox)

        self.vbox = QVBoxLayout()
        self.vbox.addLayout(self.hbox)
        self.vbox.addWidget(self.button_box)

        self.setLayout(self.vbox)

        # signals
        self.name_edit.returnPressed.connect(self.accept)
        self.btn_create.clicked.connect(self._on_create_clicked)
        self.btn_cancel.clicked.connect(self._on_cancel_clicked)

    def _on_text_edited(self, text: str) -> None:
        if text == "" or "/" in text:
            self.btn_create.setEnabled(False)
        else:
            self.btn_create.setEnabled(True)

    def _on_create_clicked(self) -> None:
        self.accept()

    def _on_cancel_clicked(self) -> None:
        self.reject()

    def set_filename(self, text: str) -> None:
        self.name_edit.setText(text)
        self.name_edit.selectAll()

    def get_filename(self) -> str:
        text: str = self.name_edit.text()
        return text


# EOF #
