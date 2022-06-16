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


from typing import TYPE_CHECKING, Optional, cast

from PyQt5.QtCore import QObject, Qt, QEvent
from PyQt5.QtWidgets import QFileDialog, QTextEdit, QDialog, QMessageBox
from PyQt5.QtGui import QCursor, QMouseEvent, QContextMenuEvent

from dirtoo.fileview.file_view_window import FileViewWindow
from dirtoo.fileview.item_context_menu import ItemContextMenu
from dirtoo.fileview.directory_context_menu import DirectoryContextMenu
from dirtoo.fileview.create_dialog import CreateDialog
from dirtoo.fileview.about_dialog import AboutDialog
from dirtoo.fileview.properties_dialog import PropertiesDialog

if TYPE_CHECKING:
    from dirtoo.file_info import FileInfo
    from dirtoo.fileview.controller import Controller


class Gui(QObject):

    def __init__(self, controller: 'Controller') -> None:
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
        """Generate a fake mouse move event to force the FileView to update
        the hover item after a menu was displayed."""

        ev = QMouseEvent(QEvent.MouseMove,
                         self._window.mapFromGlobal(QCursor.pos()),
                         Qt.NoButton,
                         Qt.NoButton,
                         Qt.NoModifier)
        self._window.file_view.mouseMoveEvent(ev)

    def on_context_menu(self, pos) -> None:
        assert self._controller.location is not None
        menu = DirectoryContextMenu(self._controller, self._controller.location)
        menu.exec(pos)
        self.fake_mouse()

    def on_item_context_menu(self, ev, item) -> None:
        if item.isSelected():
            selected_items = self._window.file_view._scene.selectedItems()
        else:
            self._controller.clear_selection()
            item.setSelected(True)
            selected_items = [item]

        menu = ItemContextMenu(self._controller, [item.fileinfo for item in selected_items])

        if ev.reason() == QContextMenuEvent.Keyboard:
            pos = self._window.file_view.mapToGlobal(
                self._window.file_view.mapFromScene(
                    item.pos() + item.boundingRect().center()))
            print(pos, item.boundingRect())
            menu.exec(pos)
        else:
            menu.exec(ev.screenPos())
        self.fake_mouse()

    def show_create_directory_dialog(self, name: Optional[str] = None) -> Optional[str]:
        dialog = CreateDialog(CreateDialog.FOLDER, self._window)
        if name is not None:
            dialog.set_name(name)
        dialog.exec()
        if dialog.result() == QDialog.Accepted:
            return dialog.get_name()
        else:
            return None

    def show_create_file_dialog(self, name: Optional[str] = None) -> Optional[str]:
        dialog = CreateDialog(CreateDialog.TEXTFILE, self._window)
        if name is not None:
            dialog.set_name(name)
        dialog.exec()
        if dialog.result() == QDialog.Accepted:
            return dialog.get_name()
        else:
            return None

    def show_error(self, title, message) -> None:
        msg = QMessageBox(self._window)
        msg.setIcon(QMessageBox.Critical)
        msg.setWindowTitle(title)
        msg.setText(message)
        msg.setStandardButtons(QMessageBox.Ok)
        msg.exec()

    def show_about_dialog(self) -> None:
        dialog = AboutDialog()
        dialog.exec()

    def show_properties_dialog(self, fileinfo: 'FileInfo'):
        dialog = PropertiesDialog(fileinfo, self._window)
        dialog.exec()


# EOF #
