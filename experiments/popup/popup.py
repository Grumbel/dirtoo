#!/usr/bin/env python3

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

import sys
import signal

from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import (QComboBox, QLineEdit, QApplication,
                             QWidget, QVBoxLayout, QListWidget)


class PopupThing(QWidget):

    def __init__(self, parent):
        super().__init__(parent,
                         Qt.Window |
                         Qt.WindowStaysOnTopHint |
                         Qt.X11BypassWindowManagerHint |
                         Qt.FramelessWindowHint)

        vbox = QVBoxLayout()
        self.listwidget = QListWidget()
        self.listwidget.addItem("Test")
        self.listwidget.addItem("Test 2")
        vbox.addWidget(self.listwidget)
        vbox.setContentsMargins(4, 4, 4, 4)
        self.setLayout(vbox)


class LineThing(QLineEdit):

    def __init__(self):
        super().__init__()

        self.widget = PopupThing(self)
        self.widget.resize(300, 400)

    def moveEvent(self, ev):
        super().moveEvent(ev)
        self.widget.move(ev.pos().x(),
                         ev.pos().y() + self.height() + 8)

    def resizeEvent(self, ev):
        super().resizeEvent(ev)
        self.widget.resize(self.width(), 400)
        self.widget.move(self.geometry().x(),
                         self.geometry().y() + self.height() + 8)

    def keyPressEvent(self, ev):
        super().keyPressEvent(ev)

        if ev.key() == Qt.Key_Up:
            self.widget.listwidget.setCurrentRow(self.widget.listwidget.currentRow() - 1)
        elif ev.key() == Qt.Key_Down:
            self.widget.listwidget.setCurrentRow(self.widget.listwidget.currentRow() + 1)
            self.widget.show()

    def show(self):
        super().show()

def main(argv):
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    app = QApplication([])

    combo = LineThing()
    # combo.setEditable(True)
    combo.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main(sys.argv)


# EOF #
