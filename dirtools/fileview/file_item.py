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


import logging

from pkg_resources import resource_filename

from PyQt5.QtCore import Qt
from PyQt5.QtGui import QDrag, QPainter, QPixmap
from PyQt5.QtWidgets import QGraphicsObject, QGraphicsItem

if False:
    from dirtools.fileview.controller import Controller  # noqa: F401
    from dirtools.fileview.file_info import FileInfo  # noqa: F401

logger = logging.getLogger(__name__)


class FileItem(QGraphicsObject):

    def __init__(self, fileinfo: 'FileInfo', controller: 'Controller') -> None:
        super().__init__()

        self.fileinfo = fileinfo
        self.controller = controller

        self.press_pos = None
        self.setFlags(QGraphicsItem.ItemIsSelectable)
        self.setAcceptHoverEvents(True)

        self.setCursor(Qt.PointingHandCursor)

    def mousePressEvent(self, ev):
        if not self.shape().contains(ev.scenePos() - self.pos()):
            # Qt is sending events that are outside the item. This
            # happens when opening a context menu on an item, moving
            # the mouse, closing it with another click and than
            # clicking again. The item on which the context menu was
            # open gets triggered even when the click is completely
            # outside the item. This sems to be due to
            # mouseMoveEvent() getting missed while the menu is open
            # and Qt triggering on the mouse position before the
            # context menu was opened. There might be better ways to
            # fix this, this is just a workaround. Hover status
            # doesn't get updated either.
            ev.ignore()
            logger.error("FileItem.mousePressEvent: broken click ignored")
            return

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
            # print("drag start")

            self.drag = QDrag(self.controller._gui._window)

            # Create the drag thumbnail
            if False:
                pix = QPixmap(self.tile_rect.width(), self.tile_rect.height())
                painter = QPainter(pix)
                self.paint(painter, None, None)
                painter.end()
                self.drag.setPixmap(pix)
                self.drag.setHotSpot(ev.pos().toPoint() - self.tile_rect.topLeft())
            else:
                pix = QPixmap("/usr/share/icons/mate/32x32/actions/gtk-dnd-multiple.png").scaled(48, 48)
                self.drag.setPixmap(pix)

            if not self.isSelected():
                self.controller.clear_selection()
                self.setSelected(True)

            mime_data = self.controller.selection_to_mimedata(uri_only=True)
            self.drag.setMimeData(mime_data)

            self.drag.setDragCursor(
                QPixmap(resource_filename("dirtools", "fileview/icons/dnd-copy.png")),
                Qt.CopyAction)
            self.drag.setDragCursor(
                QPixmap(resource_filename("dirtools", "fileview/icons/dnd-move.png")),
                Qt.MoveAction)
            self.drag.setDragCursor(
                QPixmap(resource_filename("dirtools", "fileview/icons/dnd-link.png")),
                Qt.LinkAction)

            # self.drag.actionChanged.connect(lambda action: print(action))

            # this will eat up the mouseReleaseEvent
            self.dropAction = self.drag.exec(Qt.CopyAction | Qt.MoveAction | Qt.LinkAction)

    def mouseReleaseEvent(self, ev):
        if ev.button() == Qt.LeftButton and self.press_pos is not None:
            self.click_action(new_window=False)
        elif ev.button() == Qt.MiddleButton:
            self.click_action(new_window=True)

    def click_action(self, new_window=False):
        self.on_click_animation()
        self.controller.on_click(self.fileinfo, new_window=new_window)

    def on_click_animation(self):
        pass

    def contextMenuEvent(self, ev):
        self.controller.on_item_context_menu(ev, self)

    def show_basename(self):
        pass

    def show_abspath(self):
        pass


# EOF #
