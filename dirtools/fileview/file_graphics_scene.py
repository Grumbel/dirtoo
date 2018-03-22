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
from PyQt5.QtWidgets import QGraphicsScene


class FileGraphicsScene(QGraphicsScene):

    def __init__(self):
        super().__init__()

    def dragEnterEvent(self, ev) -> None:
        print("FileGraphicsScene.dragEnterEvent()")

        if False:
            data = ev.mimeData()
            for fmt in data.formats():
                print("Format:", fmt)
                print(data.data(fmt))
                print()
            print()

        if ev.mimeData().hasUrls():
            super().dragEnterEvent(ev)
            # This function does this:
            # Q_D(QGraphicsScene);
            # d->dragDropItem = 0;
            # d->lastDropAction = Qt::IgnoreAction;
            # event->accept();
        else:
            ev.ignore()

    def dragMoveEvent(self, ev) -> None:
        print("FileGraphicsScene.dragMoveEvent()")

    def dragLeaveEvent(self, ev) -> None:
        print("FileGraphicsScene.dragLeaveEvent()")

    def dropEvent(self, ev):
        print("FileGraphicsScene.dropEvent()")

        mime_data = ev.mimeData()
        assert mime_data.hasUrls()

        urls = mime_data.urls()
        files = [url.toLocalFile() for url in urls]
        action = ev.proposedAction()

        if action == Qt.CopyAction:
            print("copy", " ".join(files))
        elif action == Qt.MoveAction:
            print("move", " ".join(files))
        elif action == Qt.LinkAction:
            print("link", " ".join(files))
        else:
            print("unsupported drop action", action)

        # self._controller.add_files([Location.from_url(url.toString()) for url in urls])


# EOF #
