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


from typing import Optional, cast

from PyQt5.QtCore import QObject, Qt, QEvent
from PyQt5.QtWidgets import QFileDialog, QTextEdit, QDialog
from PyQt5.QtGui import QCursor, QMouseEvent, QContextMenuEvent

from dirtools.fileview.file_view_window import FileViewWindow
from dirtools.fileview.controller import Controller
from dirtools.fileview.location import Location
from dirtools.fileview.item_context_menu import ItemContextMenu
from dirtools.fileview.directory_context_menu import DirectoryContextMenu
from dirtools.fileview.create_dialog import CreateDialog


class Gui(QObject):

    def __init__(self, controller: Controller) -> None:
        super().__init__()

        self._controller: Controller = controller
        self._window = FileViewWindow(self._controller)
        self._filter_help = QTextEdit()

    def get_savefilename(self) -> Optional[str]:
        options = QFileDialog.Options()
        # options |= QFileDialog.DontUseNativeDialog
        filename, kind = QFileDialog.getSaveFileName(
            self._window,
            "QFileDialog.getSaveFileName()",
            "",  # dir
            "URL List (*.urls);;Path List (*.txt);;NUL Path List (*.nlst)",
            options=options)

        if filename == "":
            return None
        else:
            return cast(str, filename)

    def show_help(self, text: str) -> None:
        self._filter_help.setText(text)
        self._filter_help.resize(480, 800)
        self._filter_help.show()

    def fake_mouse(self) -> None:
        """Generate a fake mouse move event to force the ThumbView to update
        the hover item after a menu was displayed."""

        ev = QMouseEvent(QEvent.MouseMove,
                         self._window.mapFromGlobal(QCursor.pos()),
                         Qt.NoButton,
                         Qt.NoButton,
                         Qt.NoModifier)
        self._window.thumb_view.mouseMoveEvent(ev)

    def on_context_menu(self, pos) -> None:
        menu = DirectoryContextMenu(self._controller, self._controller.location)
        menu.exec(pos)
        self.fake_mouse()

    def on_item_context_menu(self, ev, item) -> None:
        if item.isSelected():
            selected_items = self._window.thumb_view._scene.selectedItems()
        else:
            self._controller.clear_selection()
            item.setSelected(True)
            selected_items = [item]

        menu = ItemContextMenu(self._controller, [item.fileinfo for item in selected_items])

        if ev.reason() == QContextMenuEvent.Keyboard:
            pos = self._window.thumb_view.mapToGlobal(
                self._window.thumb_view.mapFromScene(
                    item.pos() + item.boundingRect().center()))
            print(pos, item.boundingRect())
            menu.exec(pos)
        else:
            menu.exec(ev.screenPos())
        self.fake_mouse()

    def create_directory(self, location: Location):
        dialog = CreateDialog(CreateDialog.FOLDER, self._controller, self._window)
        dialog.exec()
        if dialog.result() == QDialog.Accepted:
            self._controller.create_directory(location, dialog.get_name())

    def create_file(self, location: Location):
        dialog = CreateDialog(CreateDialog.TEXTFILE, self._controller, self._window)
        dialog.exec()
        if dialog.result() == QDialog.Accepted:
            self._controller.create_file(location, dialog.get_name())


# EOF #
