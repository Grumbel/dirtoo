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
from PyQt5.QtGui import QDrag, QPainter, QPixmap
from PyQt5.QtWidgets import QGraphicsObject, QGraphicsItem


class FileItem(QGraphicsObject):

    def __init__(self, fileinfo, controller):
        super().__init__()

        self.fileinfo = fileinfo
        self.controller = controller

        self.press_pos = None
        self.setFlags(QGraphicsItem.ItemIsSelectable)
        self.setAcceptHoverEvents(True)

    def mousePressEvent(self, ev):
        # PyQt will route the event to the child items when we don't
        # override this
        if ev.button() == Qt.LeftButton:
            if ev.modifiers() & Qt.ControlModifier:
                self.setSelected(not self.isSelected())
            else:
                self.press_pos = ev.pos()
        elif ev.button() == Qt.RightButton:
            pass

    def mouseMoveEvent(self, ev):
        if (ev.pos() - self.press_pos).manhattanLength() > 16:
            print("drag start")

            mime_data = QMimeData()
            mime_data.setUrls([QUrl("file://" + self.fileinfo.abspath())])
            self.drag = QDrag(self.controller.window)

            pix = QPixmap(self.tile_rect.width(), self.tile_rect.height())
            painter = QPainter(pix)
            self.paint(painter, None, None)
            self.drag.setPixmap(pix)
            painter.end()

            self.drag.setHotSpot(ev.pos().toPoint() - self.tile_rect.topLeft())

            self.setVisible(False)

            self.drag.setMimeData(mime_data)
            print("---drag start")
            self.dropAction = self.drag.exec(Qt.CopyAction)
            print("---drag done")
            # this will eat up the mouseReleaseEvent
            self.setVisible(True)

    def mouseReleaseEvent(self, ev):
        if ev.button() == Qt.LeftButton and self.press_pos is not None:
            self.click_action()
        elif ev.button() == Qt.MiddleButton:
            self.on_click_animation()
            self.controller.on_click(self.fileinfo, new_window=True)

    def click_action(self):
        self.on_click_animation()
        self.controller.on_click(self.fileinfo)

    def on_click_animation(self):
        pass

    def contextMenuEvent(self, ev):
        self.controller.on_item_context_menu(ev, self)

    def show_basename(self):
        pass

    def show_abspath(self):
        pass


# EOF #
