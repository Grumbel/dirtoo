# dirtool.py - diff tool for directories
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


from typing import cast

import html

from PyQt5.QtCore import Qt
from PyQt5.QtGui import QIcon
from PyQt5.QtWidgets import (QWidget, QDialog, QPushButton, QLayout,
                             QHBoxLayout, QVBoxLayout, QSizePolicy,
                             QDialogButtonBox, QLabel, QCheckBox)


class ConflictDialog(QDialog):

    Replace = QDialog.Accepted
    Cancel = QDialog.Rejected
    Skip = 3
    RenameSource = 4
    RenameTarget = 5

    def __init__(self, parent: QWidget) -> None:
        super().__init__()

        self._source_filename = "Source File"
        self._target_filename = "Target File"

        self._repeat_for_all: QCheckBox

        self._make_gui()

    def repeat_for_all(self) -> bool:
        return cast(bool, self._repeat_for_all.isChecked())

    def _make_file_info(self, filename: str) -> QLayout:
        # Widgets
        file_icon = QLabel()
        file_icon.setPixmap(QIcon.fromTheme("document").pixmap(48))
        file_icon.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)

        file_info = QLabel("Filename: <b>{}</b><br>"
                           "Size: 0 bytes<br>"
                           "Modified: Today".format(html.escape(filename)))
        file_info.setTextFormat(Qt.RichText)
        file_info.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

        rename_btn = QPushButton("Rename")

        # Layout
        hbox = QHBoxLayout()
        hbox.addWidget(file_icon)
        hbox.addWidget(file_info)
        hbox.addWidget(rename_btn)

        # Signals
        if filename is self._source_filename:
            rename_btn.clicked.connect(lambda: self.done(ConflictDialog.RenameSource))
        elif filename is self._target_filename:
            rename_btn.clicked.connect(lambda: self.done(ConflictDialog.RenameTarget))

        return hbox

    def _make_gui(self) -> None:
        self.setWindowTitle("Confirm to replace files")

        # Widgets
        move_icon = QLabel("Aeuau")
        move_icon.setPixmap(QIcon.fromTheme("stock_folder-move").pixmap(48))
        move_icon.setAlignment(Qt.AlignTop | Qt.AlignHCenter)

        header = QLabel("<big>This folder already contains a file <b>{}</b></big>".format(self._source_filename))
        header.setTextFormat(Qt.RichText)
        subheader = QLabel("Do you want to replace the existing file:")
        target_file_layout = self._make_file_info(self._target_filename)
        subheader2 = QLabel("with the following file?")
        source_file_layout = self._make_file_info(self._source_filename)
        repeat_for_all = QCheckBox("Repeat action for all files")
        self._repeat_for_all = repeat_for_all

        # Widgets.ButtonBox
        button_box = QDialogButtonBox(self)
        button_box.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)

        btn_replace = QPushButton("Replace")
        btn_skip = QPushButton("Skip")

        btn_cancel = button_box.addButton(QDialogButtonBox.Cancel)
        button_box.addButton(btn_replace, QDialogButtonBox.YesRole)
        button_box.addButton(btn_skip, QDialogButtonBox.NoRole)

        btn_replace.setDefault(True)

        # Layout
        subvbox = QVBoxLayout()
        subvbox.addWidget(header)
        subvbox.addWidget(subheader)
        subvbox.addLayout(target_file_layout)
        subvbox.addWidget(subheader2)
        subvbox.addLayout(source_file_layout)
        subvbox.addWidget(repeat_for_all)

        hbox = QHBoxLayout()
        hbox.addWidget(move_icon)
        hbox.addLayout(subvbox)

        vbox = QVBoxLayout()
        vbox.addLayout(hbox)
        vbox.addWidget(button_box)
        self.setLayout(vbox)

        # Signals
        btn_replace.clicked.connect(self.accept)
        btn_skip.clicked.connect(lambda: self.done(ConflictDialog.Skip))
        btn_cancel.clicked.connect(lambda: self.done(ConflictDialog.Cancel))


# EOF #
