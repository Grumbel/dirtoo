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


from PyQt5.QtCore import Qt
from PyQt5.QtGui import QIcon
from PyQt5.QtWidgets import (QWidget, QDialog, QPushButton,
                             QHBoxLayout, QVBoxLayout, QSizePolicy,
                             QDialogButtonBox, QLabel, QCheckBox)


class ConflictDialog(QDialog):

    def __init__(self, parent: QWidget) -> None:
        super().__init__()

        self._source_filename = "Source File"
        self._target_filename = "Source File"

        self._make_gui()

    def _make_file_info(self):
        file_icon = QLabel()
        file_icon.setPixmap(QIcon.fromTheme("document").pixmap(48))
        file_icon.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)

        file_info = QLabel("Size: 0 bytes\n"
                           "Modified: Today")
        file_info.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

        hbox = QHBoxLayout()
        hbox.addWidget(file_icon)
        hbox.addWidget(file_info)
        return hbox

    def _make_gui(self):
        self.setWindowTitle("Confirm to replace files")

        # Widgets
        move_icon = QLabel("Aeuau")
        move_icon.setPixmap(QIcon.fromTheme("stock_folder-move").pixmap(48))
        move_icon.setAlignment(Qt.AlignTop | Qt.AlignHCenter)

        header = QLabel("<big>This folder already contains a file <tt>{}</tt></big>".format(self._source_filename))
        header.setTextFormat(Qt.RichText)
        subheader = QLabel("Do you want to replace the existing file:")
        target_file_layout = self._make_file_info()
        subheader2 = QLabel("with the following file?")
        source_file_layout = self._make_file_info()
        repeat_for_all = QCheckBox("Repeat action for all files")

        # Widgets.ButtonBox
        self.button_box = QDialogButtonBox(self)
        self.button_box.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)

        self.btn_replace = QPushButton("Replace")
        self.btn_skip = QPushButton("Skip")

        self.button_box.addButton(QDialogButtonBox.Cancel)
        self.button_box.addButton(self.btn_replace, QDialogButtonBox.YesRole)
        self.button_box.addButton(self.btn_skip, QDialogButtonBox.NoRole)

        self.btn_replace.setDefault(True)

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
        vbox.addWidget(self.button_box)
        self.setLayout(vbox)


# EOF #
