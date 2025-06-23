# dirtoo - File and directory manipulation tools for Python
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


from typing import TYPE_CHECKING, Dict, Any, Optional

import logging
from pkg_resources import resource_filename

from PyQt6.QtCore import Qt, QPoint, QPointF, QRectF, QRect, QTimerEvent
from PyQt6.QtGui import (QColor, QPainter, QPainterPath, QImage, QDrag, QPixmap,
                         QIcon)
from PyQt6.QtWidgets import (QGraphicsObject, QGraphicsItem, QWidget,
                             QStyleOptionGraphicsItem, QGraphicsSceneMouseEvent,
                             QGraphicsSceneHoverEvent, QGraphicsSceneContextMenuEvent)

from dirtoo.filesystem.file_info import FileInfo
from dirtoo.thumbnail.thumbnail import Thumbnail, ThumbnailStatus
from dirtoo.fileview.file_item_renderer import FileItemRenderer
from dirtoo.gui.drag_widget import DragWidget

if TYPE_CHECKING:
    from dirtoo.fileview.file_view import FileView
    from dirtoo.fileview.controller import Controller

logger = logging.getLogger(__name__)


class FileItem(QGraphicsObject):

    def __init__(self, fileinfo: 'FileInfo', controller: 'Controller', file_view: 'FileView') -> None:
        logger.debug("FileItem.__init__: %s", fileinfo)
        super().__init__()

        self.fileinfo = fileinfo
        self.controller = controller

        self.press_pos: Optional[QPoint] = None
        self.setFlags(QGraphicsItem.GraphicsItemFlag.ItemIsSelectable)
        self.setAcceptHoverEvents(True)

        self.setCursor(Qt.CursorShape.PointingHandCursor)

        # self.setCacheMode(QGraphicsItem.DeviceCoordinateCache)

        self.file_view = file_view

        self._new = False
        self.hovering: bool = False
        self.animation_count = 0

        self.icon = self.make_icon()
        self.normal_thumbnail: Thumbnail = Thumbnail("normal", self)
        self.large_thumbnail: Thumbnail = Thumbnail("large", self)
        self.metadata: Optional[Dict[str, Any]] = None

        self.tile_rect: QRect
        self.thumbnail_rect: QRect
        self.bounding_rect: QRect
        self.qpainter_path: QPainterPath
        self.set_tile_size(self.file_view._mode._tile_style.tile_width, self.file_view._mode._tile_style.tile_height)
        self.animation_timer: Optional[int] = None

        self._file_is_final = True
        self._dropable = False

    def __del__(self) -> None:
        logger.debug("FileItem.__del__")

    def on_file_modified(self, fileinfo: FileInfo, final: bool = False) -> None:
        self.fileinfo = fileinfo
        self._file_is_final = final
        self._new = True

        if final:
            thumbnail = self._get_thumbnail()
            thumbnail.reset()

    def on_fileinfo_updated(self, fileinfo: FileInfo) -> None:
        self.fileinfo = fileinfo

    def set_tile_size(self, tile_width: int, tile_height: int) -> None:
        # the size of the base tile
        self.tile_rect = QRect(0, 0, int(tile_width), int(tile_height))

        # the size of the base tile
        self.thumbnail_rect = QRect(0, 0, int(tile_width), int(tile_width))

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

    def paint(self, painter: QPainter, option: QStyleOptionGraphicsItem, widget: Optional[QWidget] = None) -> None:
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
        if True or self.animation_timer is not None:  # type: ignore  # pylint: disable=R1727
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

    def make_icon(self) -> QIcon:
        icon = self.file_view.icon_from_fileinfo(self.fileinfo)
        return icon

    def hoverEnterEvent(self, ev: QGraphicsSceneHoverEvent) -> None:
        # logger.debug("FileItem.hoverEnterEvent: %s", self.fileinfo)
        self.hovering = True

        if not self.file_view.is_scrolling():
            self.controller.show_current_filename(self.fileinfo.abspath())

        self.update()

    def hoverLeaveEvent(self, ev: QGraphicsSceneHoverEvent) -> None:
        # logger.debug("FileItem.hoverLeaveEvent: %s", self.fileinfo)
        self.hovering = False

        if not self.file_view.is_scrolling():
            self.controller.show_current_filename("")

        self.update()

    def set_thumbnail_image(self, image: Optional[QImage], flavor: Optional[str]) -> None:
        thumbnail = self._get_thumbnail(flavor)
        thumbnail.set_thumbnail_image(image)
        self.update()

    def set_icon(self, icon: QIcon) -> None:
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

    def timerEvent(self, ev: QTimerEvent) -> None:
        # logger.debug("FileItem.timerEvent: %s", self.fileinfo)
        if ev.timerId() == self.animation_timer:
            self.animation_count -= 1
            self.update()
            if self.animation_count <= 0:
                assert self.animation_timer is not None
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

    def set_dropable(self, value: bool) -> None:
        self._dropable = value
        self.update()

    def mousePressEvent(self, ev: QGraphicsSceneMouseEvent) -> None:
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
        if ev.button() == Qt.MouseButton.LeftButton:
            if ev.modifiers() & Qt.KeyboardModifier.ControlModifier:
                self.setSelected(not self.isSelected())
            else:
                self.press_pos = ev.pos().toPoint()
        elif ev.button() == Qt.MouseButton.RightButton:
            pass

    def mouseMoveEvent(self, ev: QGraphicsSceneMouseEvent) -> None:
        assert self.press_pos is not None

        if (ev.pos() - QPointF(self.press_pos)).manhattanLength() > 16:
            # print("drag start")

            drag = QDrag(self.controller._gui._window)

            # Create the drag thumbnail
            if True:
                tile_pix = QPixmap(self.tile_rect.width(), self.tile_rect.height())

                painter = QPainter(tile_pix)
                self.paint(painter, None, None)  # type: ignore
                painter.end()

                pix = QPixmap(self.tile_rect.width(), self.tile_rect.height())
                pix.fill(Qt.GlobalColor.transparent)
                painter = QPainter(pix)
                painter.setOpacity(0.75)
                painter.drawPixmap(0, 0, tile_pix)
                painter.end()

                drag.setPixmap(pix)
                drag.setHotSpot(ev.pos().toPoint() - self.tile_rect.topLeft())
            else:
                pix = QPixmap(resource_filename("dirtoo", "icons/dirtoo.png")).scaled(48, 48)  # type: ignore
                drag.setPixmap(pix)

            if not self.isSelected():
                self.controller.clear_selection()
                self.setSelected(True)

            mime_data = self.controller.selection_to_mimedata(uri_only=True)
            drag.setMimeData(mime_data)

            # Qt does not allow custom drag actions officially. The
            # default drag action value is however carried through
            # even if it's invalid, but cursor changes and signals
            # like actionChanged() misbehave. The DragWidget class is
            # a workaround.
            drag_widget = DragWidget(None)  # noqa: F841

            # this will eat up the mouseReleaseEvent
            action = drag.exec(Qt.DropAction.CopyAction |
                               Qt.DropAction.MoveAction |
                               Qt.DropAction.LinkAction,
                               Qt.DropAction.CopyAction)
            print(f"drag.exec result: {action}")

    def mouseReleaseEvent(self, ev: QGraphicsSceneMouseEvent) -> None:
        if ev.button() == Qt.MouseButton.LeftButton and self.press_pos is not None:
            self.click_action(new_window=False)
        elif ev.button() == Qt.MouseButton.MiddleButton:
            self.click_action(new_window=True)

    def click_action(self, new_window: bool = False) -> None:
        self.on_click_animation()
        self.controller.on_click(self.fileinfo, new_window=new_window)

    def contextMenuEvent(self, ev: QGraphicsSceneContextMenuEvent) -> None:
        self.controller.show_item_context_menu(self, ev.screenPos())

    def show_basename(self) -> None:
        pass

    def show_abspath(self) -> None:
        pass


# EOF #
