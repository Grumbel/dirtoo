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

import html

from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import (QWidget, QDialog, QPushButton, QLayout,
                             QHBoxLayout, QVBoxLayout, QSizePolicy,
                             QDialogButtonBox, QLabel, QCheckBox, QGroupBox)

from dirtoo.file_transfer import ConflictResolution
from dirtoo.image.icon import load_icon


class ConflictDialog(QDialog):

    def __init__(self, parent: Optional[QWidget]) -> None:
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
        file_icon.setPixmap(load_icon("document").pixmap(48))
        file_icon.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

        file_info = QLabel("Filename: <b>{}</b><br>"
                           "Size: 0 bytes<br>"
                           "Modified: Today".format(html.escape(filename)))
        file_info.setTextFormat(Qt.TextFormat.RichText)
        file_info.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)

        rename_btn = QPushButton("Rename")

        # Layout
        hbox = QHBoxLayout()
        hbox.addWidget(file_icon)
        hbox.addWidget(file_info)
        hbox.addWidget(rename_btn)

        # Signals
        if filename is self._source_filename:
            rename_btn.clicked.connect(lambda: self.done(ConflictResolution.RENAME_SOURCE.value))
        elif filename is self._target_filename:
            rename_btn.clicked.connect(lambda: self.done(ConflictResolution.RENAME_TARGET.value))

        return hbox

    def _make_gui(self) -> None:
        self.setWindowTitle("Confirm to replace files")

        # Widgets
        move_icon = QLabel()
        move_icon.setPixmap(load_icon("stock_folder-move").pixmap(48))
        move_icon.setAlignment(Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignHCenter)

        header = QLabel("<big>This folder already contains a file named <b>{}</b></big>".format(self._source_filename))
        header.setTextFormat(Qt.TextFormat.RichText)

        # subheader = QLabel("Would you like to use:")
        subheader = QLabel("Replace the existing file in the destination folder?")
        source_file_layout = self._make_file_info(self._source_filename)
        source_file_widget = QGroupBox("New / Source:")
        source_file_widget.setLayout(source_file_layout)

        arrow_label = QLabel()
        arrow_label.setPixmap(load_icon("down").pixmap(24))
        arrow_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        # subheader2 = QLabel("to overwrite:")
        target_file_layout = self._make_file_info(self._target_filename)
        target_file_widget = QGroupBox("Existing / Destination:")
        target_file_widget.setLayout(target_file_layout)

        repeat_for_all = QCheckBox("Repeat action for all files")
        self._repeat_for_all = repeat_for_all

        # Widgets.ButtonBox
        button_box = QDialogButtonBox(self)
        button_box.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        btn_replace = QPushButton("Replace")
        btn_skip = QPushButton("Skip")

        btn_cancel = button_box.addButton(QDialogButtonBox.StandardButton.Cancel)
        button_box.addButton(btn_replace, QDialogButtonBox.ButtonRole.YesRole)
        button_box.addButton(btn_skip, QDialogButtonBox.ButtonRole.NoRole)

        btn_replace.setDefault(True)

        # Layout
        subvbox = QVBoxLayout()
        subvbox.addWidget(header)
        subvbox.addWidget(subheader)
        subvbox.addWidget(source_file_widget)
        subvbox.addWidget(arrow_label)
        subvbox.addWidget(target_file_widget)
        subvbox.addWidget(repeat_for_all)

        hbox = QHBoxLayout()
        hbox.addWidget(move_icon)
        hbox.addLayout(subvbox)

        vbox = QVBoxLayout()
        vbox.addLayout(hbox)
        vbox.addWidget(button_box)
        self.setLayout(vbox)

        # Signals
        btn_replace.clicked.connect(lambda: self.done(ConflictResolution.OVERWRITE.value))
        btn_skip.clicked.connect(lambda: self.done(ConflictResolution.SKIP.value))
        btn_cancel.clicked.connect(lambda: self.done(ConflictResolution.CANCEL.value))


# EOF #
