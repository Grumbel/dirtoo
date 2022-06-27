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

from PyQt5.QtCore import pyqtSignal, QEvent
from PyQt5.QtGui import QTransform, QDragEnterEvent, QDragMoveEvent
from PyQt5.QtWidgets import QGraphicsScene

from dirtoo.fileview.file_item import FileItem


class FileGraphicsScene(QGraphicsScene):

    # action, urls, destination
    sig_files_drop = pyqtSignal(int, list, object)

    def __init__(self) -> None:
        super().__init__()

        self._drag_drop_item: Optional[FileItem] = None

    def dragEnterEvent(self, ev: QDragEnterEvent) -> None:
        # print("FileGraphicsScene.dragEnterEvent()")

        if False:
            data = ev.mimeData()  # type: ignore
            for fmt in data.formats():
                print("Format:", fmt)
                print(data.data(fmt))
                print()
            print()

        if ev.mimeData().hasUrls():
            assert self._drag_drop_item is None
            self._drag_drop_item = None
            ev.accept()
        else:
            ev.ignore()

    def dragMoveEvent(self, ev: QDragMoveEvent) -> None:
        # print("FileGraphicsScene.dragMoveEvent()")
        ev.acceptProposedAction()

        transform = QTransform()
        item = self.itemAt(ev.scenePos(), transform)
        if item != self._drag_drop_item:
            if self._drag_drop_item is not None:
                self._drag_drop_item.set_dropable(False)

            if isinstance(item, FileItem) and item.fileinfo.isdir():
                self._drag_drop_item = item
                self._drag_drop_item.set_dropable(True)
            else:
                self._drag_drop_item = None

    def dragLeaveEvent(self, ev: QEvent) -> None:
        # print("FileGraphicsScene.dragLeaveEvent()")
        ev.accept()

        if self._drag_drop_item is not None:
            self._drag_drop_item.set_dropable(False)
            self._drag_drop_item = None

    def dropEvent(self, ev: QEvent) -> None:
        # print("FileGraphicsScene.dropEvent()")
        ev.accept()

        mime_data = ev.mimeData()
        assert mime_data.hasUrls()

        urls = mime_data.urls()
        assert ev.dropAction() == ev.proposedAction()
        action = ev.dropAction()

        if self._drag_drop_item is not None:
            self.sig_files_drop.emit(action, urls, self._drag_drop_item.fileinfo.location())
            self._drag_drop_item.set_dropable(False)
            self._drag_drop_item = None
        else:
            self.sig_files_drop.emit(action, urls, None)

        # self._controller.add_files([Location.from_url(url.toString()) for url in urls])


# EOF #
