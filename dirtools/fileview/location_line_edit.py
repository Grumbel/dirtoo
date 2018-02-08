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


import os

from PyQt5.QtCore import Qt
from PyQt5.QtGui import QPalette
from PyQt5.QtWidgets import QLineEdit


class LocationLineEdit(QLineEdit):

    def __init__(self, controller):
        super().__init__()
        self.controller = controller
        self.is_unused = True
        self.returnPressed.connect(self.on_return_pressed)
        self.textEdited.connect(self.on_text_edited)

    def keyPressEvent(self, ev):
        super().keyPressEvent(ev)
        if ev.key() == Qt.Key_Escape:
            self.setText(self.controller.location)
            self.on_text_edited(self.controller.location)
            self.controller.window.thumb_view.setFocus()

    def focusInEvent(self, ev):
        super().focusInEvent(ev)

        if ev.reason() != Qt.ActiveWindowFocusReason and self.is_unused:
            self.setText("")

        p = self.palette()
        p.setColor(QPalette.Text, Qt.black)
        self.setPalette(p)

    def focusOutEvent(self, ev):
        super().focusOutEvent(ev)

        if ev.reason() != Qt.ActiveWindowFocusReason and self.is_unused:
            self.set_unused_text()

    def on_text_edited(self, text):
        p = self.palette()
        if os.path.exists(self.text()):
            p.setColor(QPalette.Text, Qt.black)
        else:
            p.setColor(QPalette.Text, Qt.red)
        self.setPalette(p)

    def on_return_pressed(self):
        if os.path.exists(self.text()):
            self.controller.set_location(self.text())

    def set_location(self, text):
        self.is_unused = False
        p = self.palette()
        p.setColor(QPalette.Text, Qt.black)
        self.setPalette(p)
        self.setText(text)

    def set_unused_text(self):
        self.is_unused = True
        p = self.palette()
        p.setColor(QPalette.Text, Qt.gray)
        self.setPalette(p)
        self.setText("no location selected, file list mode is active")


# EOF #
