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


import logging

from PyQt5.QtCore import Qt
from PyQt5.QtGui import QPalette
from PyQt5.QtWidgets import QLineEdit

from dirtools.fileview.controller import Controller
from dirtools.fileview.location import Location

logger = logging.getLogger(__name__)


class LocationLineEdit(QLineEdit):

    def __init__(self, controller: Controller) -> None:
        super().__init__()

        self.controller = controller
        self.is_unused = True
        self.returnPressed.connect(self.on_return_pressed)
        self.textEdited.connect(self.on_text_edited)

    def keyPressEvent(self, ev) -> None:
        super().keyPressEvent(ev)
        if ev.key() == Qt.Key_Escape:
            self.setText(self.controller.location.as_url())
            self.on_text_edited(self.text())
            self.controller.window.thumb_view.setFocus()

    def focusInEvent(self, ev) -> None:
        super().focusInEvent(ev)

        if ev.reason() != Qt.ActiveWindowFocusReason and self.is_unused:
            self.setText("")

        p = self.palette()
        p.setColor(QPalette.Text, Qt.black)
        self.setPalette(p)

    def focusOutEvent(self, ev) -> None:
        super().focusOutEvent(ev)

        if ev.reason() != Qt.ActiveWindowFocusReason and self.is_unused:
            self.set_unused_text()

    def on_text_edited(self, text) -> None:
        p = self.palette()

        try:
            location = Location.from_url(text)
            if location.exists():
                p.setColor(QPalette.Text, Qt.black)
            else:
                p.setColor(QPalette.Text, Qt.red)
        except Exception:
            p.setColor(QPalette.Text, Qt.red)

        self.setPalette(p)

    def on_return_pressed(self) -> None:
        try:
            location = Location.from_url(self.text())
        except Exception:
            logger.warning("unparsable location entered: %s", self.text())
        else:
            self.controller.set_location(location)

    def set_location(self, location: Location) -> None:
        self.is_unused = False
        p = self.palette()
        p.setColor(QPalette.Text, Qt.black)
        self.setPalette(p)
        self.setText(location.as_url())

    def set_unused_text(self) -> None:
        self.is_unused = True
        p = self.palette()
        p.setColor(QPalette.Text, Qt.gray)
        self.setPalette(p)
        self.setText("no location selected, file list mode is active")


# EOF #
