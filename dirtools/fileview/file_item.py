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


from typing import Dict, Any, Optional

from pkg_resources import resource_filename
import logging

from PyQt5.QtCore import Qt, QRectF, QRect
from PyQt5.QtGui import QColor, QPainter, QPainterPath, QImage, QDrag, QPixmap

from dirtools.fileview.file_info import FileInfo
from dirtools.fileview.thumbnail import Thumbnail, ThumbnailStatus
from dirtools.fileview.file_item_renderer import FileItemRenderer
from PyQt5.QtWidgets import QGraphicsObject, QGraphicsItem

if False:
    from dirtools.fileview.controller import Controller  # noqa: F401

logger = logging.getLogger(__name__)


class FileItem(QGraphicsObject):

    def __init__(self, fileinfo: 'FileInfo', controller: 'Controller', file_view) -> None:
        logger.debug("FileItem.__init__: %s", fileinfo)
        super().__init__()

        self.fileinfo = fileinfo
        self.controller = controller

        self.press_pos = None
        self.setFlags(QGraphicsItem.ItemIsSelectable)
        self.setAcceptHoverEvents(True)

        self.setCursor(Qt.PointingHandCursor)

        # self.setCacheMode(QGraphicsItem.DeviceCoordinateCache)

        self.file_view = file_view

        self._new = False
        self.hovering: bool = False
        self.animation_count = 0

        self.icon = self.make_icon()
        self.normal_thumbnail: Thumbnail = Thumbnail("normal", self)
        self.large_thumbnail: Thumbnail = Thumbnail("large", self)
        self.metadata: Optional[Dict[str, Any]] = None

        self.set_tile_size(self.file_view._mode._tile_style.tile_width, self.file_view._mode._tile_style.tile_height)
        self.animation_timer: Optional[int] = None

        self._file_is_final = True
        self._dropable = False

    def on_file_modified(self, fileinfo: FileInfo, final=False):
        self.fileinfo = fileinfo
        self._file_is_final = final
        self._new = True

        if final:
            thumbnail = self._get_thumbnail()
            thumbnail.reset()

    def on_fileinfo_updated(self, fileinfo: FileInfo):
        self.fileinfo = fileinfo

    def set_tile_size(self, tile_width: int, tile_height: int) -> None:
        # the size of the base tile
        self.tile_rect = QRect(0, 0, tile_width, tile_height)

        # the size of the base tile
        self.thumbnail_rect = QRect(0, 0, tile_width, tile_width)

        # the bounding rect for drawing, it's a little bigger to allow
        # border effects and such
        self.bounding_rect = QRect(self.tile_rect.x() - 3,
                                   self.tile_rect.y() - 3,
                                   self.tile_rect.width() + 6,
                                   self.tile_rect.height() + 6)

        # the path used for collision detection
        self.qpainter_path = QPainterPath()
        self.qpainter_path.addRect(0, 0, tile_width, tile_height)

    def prepare(self) -> None:
        if self._file_is_final:
            if self.metadata is None:
                self.controller.request_metadata(self.fileinfo)
                self.metadata = {}

            thumbnail = self._get_thumbnail()
            if thumbnail.status == ThumbnailStatus.INITIAL:
                thumbnail.request()

    def paint(self, painter, option, widget) -> None:
        # logger.debug("FileItem.paint: %s", self.fileinfo)

        if not self.file_view.is_scrolling():
            self.prepare()

        if self.animation_timer is None:
            # FIXME: calling os.getuid() is slow, so use 1000 as
            # workaround for now
            if self.fileinfo.uid() == 1000:
                bg_color = QColor(192 + 48, 192 + 48, 192 + 48)
            elif self.fileinfo.uid() == 0:
                bg_color = QColor(192 + 32, 176, 176)
            else:
                bg_color = QColor(176, 192 + 32, 176)
        else:
            bg_color = QColor(192 + 32 - 10 * self.animation_count,
                              192 + 32 - 10 * self.animation_count,
                              192 + 32 - 10 * self.animation_count)

        # background rectangle
        if True or self.animation_timer is not None:
            painter.fillRect(0, 0,
                             self.tile_rect.width(),
                             self.tile_rect.height(),
                             bg_color)

        # hover rectangle
        if self.hovering and self.animation_timer is None:
            painter.fillRect(-4,
                             -4,
                             self.tile_rect.width() + 8,
                             self.tile_rect.height() + 8,
                             bg_color if self.isSelected() else QColor(192, 192, 192))

        if self._dropable:
            painter.fillRect(0,
                             0,
                             self.tile_rect.width(),
                             self.tile_rect.height(),
                             QColor(164, 164, 164))

        renderer = FileItemRenderer(self)
        renderer.render(painter)

    def make_icon(self):
        icon = self.file_view.icon_from_fileinfo(self.fileinfo)
        return icon

    def hoverEnterEvent(self, ev):
        # logger.debug("FileItem.hoverEnterEvent: %s", self.fileinfo)
        self.hovering = True

        if not self.file_view.is_scrolling():
            self.controller.show_current_filename(self.fileinfo.abspath())

        self.update()

    def hoverLeaveEvent(self, ev):
        # logger.debug("FileItem.hoverLeaveEvent: %s", self.fileinfo)
        self.hovering = False

        if not self.file_view.is_scrolling():
            self.controller.show_current_filename("")

        self.update()

    def set_thumbnail_image(self, image: QImage, flavor) -> None:
        thumbnail = self._get_thumbnail(flavor)
        thumbnail.set_thumbnail_image(image)
        self.update()

    def set_icon(self, icon) -> None:
        self.icon = icon

    def boundingRect(self) -> QRectF:
        return QRectF(self.bounding_rect)

    def shape(self) -> QPainterPath:
        return self.qpainter_path

    def reload(self) -> None:
        self.normal_thumbnail = Thumbnail("normal", self)
        self.large_thumbnail = Thumbnail("large", self)
        self.update()

    def reload_thumbnail(self) -> None:
        self.reload()

    def reload_metadata(self) -> None:
        self.metadata = None

    def on_click_animation(self) -> None:
        if self.animation_timer is not None:
            self.killTimer(self.animation_timer)

        self.animation_timer = self.startTimer(30)
        self.animation_count = 10
        self.update()

    def timerEvent(self, ev) -> None:
        # logger.debug("FileItem.timerEvent: %s", self.fileinfo)
        if ev.timerId() == self.animation_timer:
            self.animation_count -= 1
            self.update()
            if self.animation_count <= 0:
                self.killTimer(self.animation_timer)
                self.animation_timer = None
        else:
            assert False, "timer foobar"

    def _get_thumbnail(self, flavor: Optional[str] = None) -> Thumbnail:
        if flavor is None:
            flavor = self.file_view.flavor

        if flavor == "normal":
            return self.normal_thumbnail
        elif flavor == "large":
            return self.large_thumbnail
        else:
            return self.large_thumbnail

    def set_dropable(self, value):
        self._dropable = value
        self.update()

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

    def contextMenuEvent(self, ev):
        self.controller.on_item_context_menu(ev, self)

    def show_basename(self):
        pass

    def show_abspath(self):
        pass


# EOF #
