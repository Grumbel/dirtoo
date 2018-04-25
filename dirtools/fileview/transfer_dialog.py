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
                             QDialogButtonBox, QLabel, QCheckBox,
                             QListWidget, QAbstractScrollArea)


class TransferDialog(QDialog):

    Cancel = QDialog.Rejected
    Move = 2
    Copy = 3

    def __init__(self, parent: QWidget) -> None:
        super().__init__()

        self._source_files = [
            "/home/juser/test.txt",
            "/home/juser/README.md",
            "/home/juser/NotAFile.c",
            "/home/juser/NotAFile.c",
        ]
        self._target_directory = "/home/juser/Target Directory"

        self._make_gui()

    def _make_file_info(self, filename: str) -> QLayout:
        # Widgets
        file_icon = QLabel()
        file_icon.setPixmap(QIcon.fromTheme("folder").pixmap(48))
        file_icon.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)

        file_info = QLabel("Filename: <b>{}</b><br>"
                           "Size: 0 bytes<br>"
                           "Modified: Today".format(html.escape(filename)))
        file_info.setTextFormat(Qt.RichText)
        file_info.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

        # Layout
        hbox = QHBoxLayout()
        hbox.addWidget(file_icon)
        hbox.addWidget(file_info)

        return hbox

    def _make_gui(self) -> None:
        self.setWindowTitle("Confirm file transfer")

        # Widgets
        move_icon = QLabel()
        move_icon.setPixmap(QIcon.fromTheme("stock_folder-move").pixmap(48))
        move_icon.setAlignment(Qt.AlignTop | Qt.AlignHCenter)

        header = QLabel("<big>A file transfer was requested.</big>")
        header.setTextFormat(Qt.RichText)
        subheader = QLabel("Do you want to transfer the following files:")

        source_files_list = QListWidget()
        source_files_list.setSizeAdjustPolicy(QAbstractScrollArea.AdjustToContents)
        for source_file in self._source_files:
            source_files_list.addItem(source_file)

        subheader2 = QLabel("into the target directory:")
        target_directory_layout = self._make_file_info(self._target_directory)

        # Widgets.ButtonBox
        button_box = QDialogButtonBox(self)
        button_box.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)

        btn_cancel = button_box.addButton(QDialogButtonBox.Cancel)

        btn_copy = QPushButton(QIcon.fromTheme("stock_folder-copy"), "Copy Files")
        btn_move = QPushButton(QIcon.fromTheme("stock_folder-move"), "Move Files")
        button_box.addButton(btn_move, QDialogButtonBox.AcceptRole)
        button_box.addButton(btn_copy, QDialogButtonBox.AcceptRole)
        btn_move.setDefault(True)

        # Layout
        subvbox = QVBoxLayout()
        subvbox.addWidget(header)
        subvbox.addWidget(subheader)
        subvbox.addWidget(source_files_list)
        subvbox.addWidget(subheader2)
        subvbox.addLayout(target_directory_layout)

        hbox = QHBoxLayout()
        hbox.addWidget(move_icon)
        hbox.addLayout(subvbox)

        vbox = QVBoxLayout()
        vbox.addLayout(hbox)
        vbox.addWidget(button_box)
        self.setLayout(vbox)

        # Signals
        btn_copy.clicked.connect(lambda: self.done(TransferDialog.Copy))
        btn_move.clicked.connect(lambda: self.done(TransferDialog.Move))
        btn_cancel.clicked.connect(lambda: self.done(TransferDialog.Cancel))


# EOF #
