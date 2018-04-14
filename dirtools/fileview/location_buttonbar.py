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


from typing import Optional, List

from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import QWidget, QHBoxLayout, QPushButton, QSizePolicy
import sip

from dirtools.fileview.location import Location

if False:
    from dirtools.fileview.controller import Controller  # noqa: F401


class LocationButtonBar(QWidget):

    def __init__(self, controller: 'Controller') -> None:
        super().__init__()

        self._controller = controller
        self._location: Option[Location] = None

    def set_location(self, location: Location) -> None:
        self._location = location
        self._build_buttons()

    def _build_buttons(self) -> None:
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)
        ancestry = self._location.ancestry()

        for location in ancestry:
            print(location.basename())
            button = QPushButton(location.basename())
            button.clicked.connect(lambda checked, location=location: self._controller.set_location(location))
            button.setSizePolicy(QSizePolicy.Maximum, QSizePolicy.Maximum)
            layout.addWidget(button, Qt.AlignLeft)

        button.setDown(True)

        layout.addStretch()

        self._clearLayout()
        self.setLayout(layout)

    def _clearLayout(self):
        # See: https://stackoverflow.com/questions/4528347/clear-all-widgets-in-a-layout-in-pyqt
        if self.layout() is not None:
            old_layout = self.layout()
            for i in reversed(range(old_layout.count())):
                w = old_layout.itemAt(i).widget()
                if w is not None:
                    w.setParent(None)
            sip.delete(old_layout)


# EOF #
