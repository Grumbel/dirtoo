# dirtool.py - diff tool for directories
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


from PyQt5.QtCore import Qt, QMimeData, QUrl
from PyQt5.QtGui import QDrag
from PyQt5.QtWidgets import (
    QGraphicsItemGroup,
    QMenu
)


class FileItem(QGraphicsItemGroup):

    def __init__(self, fileinfo, controller):
        super().__init__()
        self.fileinfo = fileinfo
        self.thumbnail = None
        self.controller = controller
        self.setAcceptHoverEvents(True)

        self.text = None
        self.make_items()
        self.show_abspath()

        self.press_pos = None
        self.dragging = False

    def mousePressEvent(self, ev):
        # PyQt will route the event to the child items when we don't
        # override this
        if ev.button() == Qt.LeftButton:
            self.press_pos = ev.pos()
        elif ev.button() == Qt.RightButton:
            pass

    def mouseMoveEvent(self, ev):
        if not self.dragging and (ev.pos() - self.press_pos).manhattanLength() > 16:
            print("drag start")
            self.dragging = True

            mime_data = QMimeData()
            mime_data.setUrls([QUrl("file://" + self.fileinfo.abspath)])
            self.drag = QDrag(self.controller.window)
            self.drag.setPixmap(self.pixmap_item.pixmap())
            self.drag.setMimeData(mime_data)
            # drag.setHotSpot(e.pos() - self.select_rect().topLeft())
            self.dropAction = self.drag.exec_(Qt.CopyAction)

    def mouseReleaseEvent(self, ev):
        if ev.button() == Qt.LeftButton:
            if self.dragging:
                pass
            else:
                self.controller.on_click(self.fileinfo)

            self.dragging = False

    def contextMenuEvent(self, ev):
        menu = QMenu()
        menu.addAction("Open with Default",
                       lambda: self.controller.on_click(self.fileinfo))
        menu.addAction("Open with Other",
                       lambda: self.controller.on_click(self.fileinfo))
        menu.addAction("Properties...")
        menu.exec(ev.screenPos())

    def show_basename(self):
        pass

    def show_abspath(self):
        pass


# EOF #
