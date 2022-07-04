# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2017 Ingo Ruhnke <grumbel@gmail.com>
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


from enum import Enum

from PyQt5.QtCore import Qt
from PyQt5.QtGui import QPalette, QColor
from PyQt5.QtWidgets import QWidget, QLabel, QPushButton, QHBoxLayout, QSizePolicy

from dirtoo.image.icon import load_icon


class MessageSeverity(Enum):

    INFO = 0
    WARNING = 1
    ERROR = 2


class MessageArea(QWidget):

    def __init__(self) -> None:
        super().__init__()

        # self.setStyleSheet("background-color: #fcf7b6;")
        pal = QPalette()
        pal.setColor(QPalette.Background, QColor("#fcf7b6"))
        self.setPalette(pal)
        self.setAutoFillBackground(True)

        self._icons = [
            load_icon("dialog-information"),
            load_icon("dialog-warning"),
            load_icon("dialog-error")
        ]

        self._icon_label = QLabel()
        self._icon_label.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)
        self._icon_label.setMargin(8)

        self._label = QLabel()
        self._label.setWordWrap(True)
        # self._label.setAlignment(Qt.AlignHCenter)
        self._label.setMargin(4)
        self._label.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self._label.setTextInteractionFlags(Qt.TextSelectableByMouse)

        self._close_btn = QPushButton(load_icon("window-close"), "")
        self._close_btn.setFlat(True)
        self._close_btn.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)
        self._close_btn.clicked.connect(self._on_close_btn_triggerd)

        self._hbox = QHBoxLayout()
        self._hbox.setContentsMargins(0, 0, 0, 0)
        # self._hbox.setContentsMargins(0,0,0,0)
        self._hbox.addWidget(self._icon_label, 0, Qt.AlignTop)
        self._hbox.addWidget(self._label)
        self._hbox.addWidget(self._close_btn, 0, Qt.AlignTop)

        self.setLayout(self._hbox)

    def _on_close_btn_triggerd(self) -> None:
        self.hide()

    def show_message(self, severity: MessageSeverity, message: str) -> None:
        icon = self._icons[severity.value]
        self._icon_label.setPixmap(icon.pixmap(32))

        self._label.setText(message)

        self.show()

    def show_info(self, message: str) -> None:
        self.show_message(MessageSeverity.INFO, message)

    def show_warning(self, message: str) -> None:
        self.show_message(MessageSeverity.WARNING, message)

    def show_error(self, message: str) -> None:
        self.show_message(MessageSeverity.ERROR, message)


# EOF #
