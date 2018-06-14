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


from typing import TYPE_CHECKING, Optional, List, Tuple

from PyQt5.QtCore import Qt, QSize
from PyQt5.QtWidgets import QWidget, QHBoxLayout, QSizePolicy
from PyQt5.QtGui import QIcon
import sip

from dirtools.fileview.location import Location
from dirtools.fileview.push_button import PushButton
from dirtools.fileview.item_context_menu import ItemContextMenu

if TYPE_CHECKING:
    from dirtools.fileview.controller import Controller  # noqa: F401


class LocationButton(PushButton):

    def __init__(self, controller: 'Controller', location: Location, *args) -> None:
        super().__init__(*args)

        self._controller = controller
        self._location = location

        self.setAcceptDrops(True)

    def dragEnterEvent(self, ev) -> None:
        if ev.mimeData().hasUrls():
            ev.accept()
        else:
            ev.ignore()

    def dragLeaveEvent(self, ev) -> None:
        ev.accept()

    def dropEvent(self, ev):
        ev.accept()

        mime_data = ev.mimeData()
        assert mime_data.hasUrls()

        urls = mime_data.urls()
        assert ev.dropAction() == ev.proposedAction()
        action = ev.dropAction()

        self._controller.on_files_drop(action, urls, self._location)


class LocationButtonBar(QWidget):

    def __init__(self, controller: 'Controller') -> None:
        super().__init__()

        self._controller = controller
        self._location: Optional[Location] = None
        self._buttons: List[Tuple[Location, LocationButton]] = []

    def set_location(self, location: Location) -> None:
        self._location = location

        found = False
        for btn_loc, button in self._buttons:
            if location == btn_loc:
                button.setDown(True)
                found = True
            else:
                button.setDown(False)

        if not found:
            self._build_buttons()

    def _build_buttons(self) -> None:
        if self._location is None:
            return

        self._buttons.clear()

        layout = QHBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        ancestry = self._location.ancestry()

        for location in ancestry:
            basename = location.basename()
            if basename == "":
                if location.protocol() == "file":
                    button = LocationButton(self._controller, location, QIcon.fromTheme("drive-harddisk"), "")
                    button.setIconSize(QSize(16, 16))
                else:
                    button = LocationButton(self._controller, location, location.protocol() + "://")
            else:
                button = LocationButton(self._controller, location, basename)
                button.setStyleSheet("padding: 3px 4px;")

            button.setContextMenuPolicy(Qt.CustomContextMenu)
            button.customContextMenuRequested.connect(lambda pos, button=button, location=location:
                                                      self._on_button_context_menu(button, location, pos))

            button.clicked.connect(lambda checked, location=location:
                                   self._controller.set_location(location))
            button.middle_clicked.connect(lambda location=location:
                                          self._controller.new_controller().set_location(location))

            button.setSizePolicy(QSizePolicy.Maximum, QSizePolicy.Maximum)
            button.setMinimumWidth(4)
            self._buttons.append((location, button))

            layout.addWidget(button, Qt.AlignCenter)

        button.setDown(True)

        layout.addStretch()

        self._clearLayout()
        self.setLayout(layout)

    def _on_button_context_menu(self, button: LocationButton, location: Location, pos) -> None:
        fileinfo = self._controller.app.vfs.get_fileinfo(location.pure())
        menu = ItemContextMenu(self._controller, [fileinfo])
        menu.exec(button.mapToGlobal(pos))

    def _clearLayout(self):
        # See: https://stackoverflow.com/questions/4528347/clear-all-widgets-in-a-layout-in-pyqt
        if self.layout() is not None:
            old_layout = self.layout()
            for i in reversed(range(old_layout.count())):
                w = old_layout.itemAt(i).widget()
                if w is not None:
                    w.setParent(None)
            sip.delete(old_layout)

    def mousePressEvent(self, ev):
        print("LocationButtonBar: press event", ev)
        self._controller.show_location_toolbar()


# EOF #
