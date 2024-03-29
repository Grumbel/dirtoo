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


from typing import Optional, Sequence

import html

from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import (QWidget, QDialog, QPushButton, QLayout,
                             QHBoxLayout, QVBoxLayout, QSizePolicy,
                             QDialogButtonBox, QLabel, QListWidget,
                             QAbstractScrollArea, QGroupBox)

from dirtoo.image.icon import load_icon


class TransferRequestDialog(QDialog):

    Cancel = QDialog.DialogCode.Rejected
    Move = 2
    Copy = 3
    Link = 4

    def __init__(self, source_files: Sequence[str], target_directory: str, parent: Optional[QWidget]) -> None:
        super().__init__()

        self._source_files = source_files
        self._target_directory = target_directory

        self._make_gui()

    def _make_file_info(self, filename: str) -> QLayout:
        # Widgets
        file_icon = QLabel()
        file_icon.setPixmap(load_icon("folder").pixmap(48))
        file_icon.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

        file_info = QLabel("Filename: <b>{}</b><br>"
                           "Size: 0 bytes<br>"
                           "Modified: Today".format(html.escape(filename)))
        file_info.setTextFormat(Qt.TextFormat.RichText)
        file_info.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)

        # Layout
        hbox = QHBoxLayout()
        hbox.addWidget(file_icon)
        hbox.addWidget(file_info)

        return hbox

    def _make_gui(self) -> None:
        self.setWindowTitle("Confirm file transfer")

        # Widgets
        move_icon = QLabel()
        move_icon.setPixmap(load_icon("stock_folder-move").pixmap(48))
        move_icon.setAlignment(Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignHCenter)

        header = QLabel("<big>A file transfer was requested.</big>")
        header.setTextFormat(Qt.TextFormat.RichText)
        subheader = QLabel("Do you want to transfer the following files to the given directory?")

        source_files_widget = QGroupBox("Sources:")
        source_files_list = QListWidget()
        source_files_list.setSizeAdjustPolicy(QAbstractScrollArea.SizeAdjustPolicy.AdjustToContents)
        for source_file in self._source_files:
            source_files_list.addItem(source_file)

        box = QVBoxLayout()
        box.addWidget(source_files_list)
        source_files_widget.setLayout(box)

        arrow_label = QLabel()
        arrow_label.setPixmap(load_icon("down").pixmap(24))
        arrow_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        target_directory_layout = self._make_file_info(self._target_directory)
        target_directory_widget = QGroupBox("Destination:")
        target_directory_widget.setLayout(target_directory_layout)

        # Widgets.ButtonBox
        button_box = QDialogButtonBox(self)
        button_box.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        btn_cancel = button_box.addButton(QDialogButtonBox.StandardButton.Cancel)

        btn_copy = QPushButton(load_icon("stock_folder-copy"), "Copy Files")
        btn_move = QPushButton(load_icon("stock_folder-move"), "Move Files")
        btn_link = QPushButton(load_icon("stock_folder-move"), "Link Files")
        button_box.addButton(btn_move, QDialogButtonBox.ButtonRole.AcceptRole)
        button_box.addButton(btn_copy, QDialogButtonBox.ButtonRole.AcceptRole)
        button_box.addButton(btn_link, QDialogButtonBox.ButtonRole.AcceptRole)
        btn_move.setDefault(True)

        # Layout
        subvbox = QVBoxLayout()
        subvbox.addWidget(header)
        subvbox.addWidget(subheader)
        subvbox.addWidget(source_files_widget)
        subvbox.addWidget(arrow_label)
        subvbox.addWidget(target_directory_widget)

        hbox = QHBoxLayout()
        hbox.addWidget(move_icon)
        hbox.addLayout(subvbox)

        vbox = QVBoxLayout()
        vbox.addLayout(hbox)
        vbox.addWidget(button_box)
        self.setLayout(vbox)

        # Signals
        btn_copy.clicked.connect(lambda: self.done(TransferRequestDialog.Copy))
        btn_move.clicked.connect(lambda: self.done(TransferRequestDialog.Move))
        btn_link.clicked.connect(lambda: self.done(TransferRequestDialog.Link))
        btn_cancel.clicked.connect(lambda: self.done(TransferRequestDialog.Cancel))


# EOF #
