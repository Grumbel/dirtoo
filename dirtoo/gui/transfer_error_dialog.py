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

import html

from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import (QWidget, QDialog, QPushButton,
                             QHBoxLayout, QVBoxLayout, QSizePolicy,
                             QDialogButtonBox, QLabel, QPlainTextEdit)

from dirtoo.image.icon import load_icon


class TransferErrorDialog(QDialog):

    Cancel = QDialog.Rejected
    Skip = QDialog.Accepted
    Retry = 3

    def __init__(self, source_file: str, target_file: str, error_msg: str, parent: Optional[QWidget]) -> None:
        super().__init__()

        self._source_file = source_file
        self._target_file = target_file
        self._error_msg = error_msg

        self._make_gui()

    def _make_gui(self) -> None:
        self.setWindowTitle("Error on file transfer")

        # Widgets
        move_icon = QLabel()
        move_icon.setPixmap(load_icon("error").pixmap(48))
        move_icon.setAlignment(Qt.AlignTop | Qt.AlignHCenter)

        header = QLabel("<big>An error occured while accessing '{}'</big>".format(html.escape(self._source_file)))
        header.setTextFormat(Qt.RichText)

        error_widget = QPlainTextEdit()
        error_widget.setReadOnly(True)
        error_widget.setPlainText(self._error_msg)

        subheader = QLabel("Do you want to skip it?")

        # Widgets.ButtonBox
        button_box = QDialogButtonBox(self)
        button_box.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)

        btn_cancel = button_box.addButton(QDialogButtonBox.Cancel)
        btn_skip = QPushButton("Skip")
        btn_retry = QPushButton("Retry")

        button_box.addButton(btn_skip, QDialogButtonBox.AcceptRole)
        button_box.addButton(btn_retry, QDialogButtonBox.AcceptRole)
        btn_skip.setDefault(True)

        # Layout
        subvbox = QVBoxLayout()
        subvbox.addWidget(header)
        subvbox.addWidget(error_widget)
        subvbox.addWidget(subheader)

        hbox = QHBoxLayout()
        hbox.addWidget(move_icon)
        hbox.addLayout(subvbox)

        vbox = QVBoxLayout()
        vbox.addLayout(hbox)
        vbox.addWidget(button_box)
        self.setLayout(vbox)

        # Signals
        btn_skip.clicked.connect(lambda: self.done(TransferErrorDialog.Skip))
        btn_retry.clicked.connect(lambda: self.done(TransferErrorDialog.Retry))
        btn_cancel.clicked.connect(lambda: self.done(TransferErrorDialog.Cancel))


# EOF #
