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

import logging

from PyQt5.QtCore import QRectF, QRect
from PyQt5.QtGui import QColor, QPainterPath, QImage

from dirtools.fileview.file_item import FileItem
from dirtools.fileview.file_info import FileInfo
from dirtools.fileview.thumbnail import Thumbnail, ThumbnailStatus
from dirtools.fileview.thumb_file_item_renderer import ThumbFileItemRenderer

logger = logging.getLogger(__name__)


class ThumbFileItem(FileItem):

    def __init__(self, fileinfo: FileInfo, controller, thumb_view) -> None:
        logger.debug("ThumbFileItem.__init__: %s", fileinfo)
        super().__init__(fileinfo, controller)
        # self.setCacheMode(QGraphicsItem.DeviceCoordinateCache)

        self.thumb_view = thumb_view

        self._new = False
        self.hovering: bool = False
        self.animation_count = 0

        self.icon = self.make_icon()
        self.normal_thumbnail: Thumbnail = Thumbnail("normal", self)
        self.large_thumbnail: Thumbnail = Thumbnail("large", self)
        self.metadata: Optional[Dict[str, Any]] = None

        self.set_tile_size(self.thumb_view._tile_style.tile_width, self.thumb_view._tile_style.tile_height)
        self.animation_timer: Optional[int] = None

        self._file_is_final = True

    def set_fileinfo(self, fileinfo: FileInfo, final=False, update=False):
        self.fileinfo = fileinfo
        self._file_is_final = final
        if not update:
            self._new = True
        if final:
            thumbnail = self._get_thumbnail()
            thumbnail.reset()

    def set_tile_size(self, tile_width: int, tile_height: int) -> None:
        # the size of the base tile
        self.tile_rect = QRect(0, 0, tile_width, tile_height)

        # the size of the base tile
        self.thumbnail_rect = QRect(0, 0, tile_width, tile_width)

        # the bounding rect for drawing, it's a little bigger to allow
        # border effects and such
        self.bounding_rect = QRect(self.tile_rect.x() - 8,
                                   self.tile_rect.y() - 8,
                                   self.tile_rect.width() + 16,
                                   self.tile_rect.height() + 16)

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
        # logger.debug("ThumbFileItem.paint: %s", self.fileinfo)

        self.prepare()

        if self.animation_timer is None:
            # FIXME: calling os.getuid() is slow, so use 1000 as
            # workaround for now
            if 1000 == self.fileinfo.uid():
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

        renderer = ThumbFileItemRenderer(self)
        renderer.render(painter)

    def make_icon(self):
        icon = self.thumb_view.icon_from_fileinfo(self.fileinfo)
        return icon

    def hoverEnterEvent(self, ev):
        # logger.debug("ThumbFileItem.hoverEnterEvent: %s", self.fileinfo)
        self.hovering = True
        self.controller.show_current_filename(self.fileinfo.abspath())
        self.update()

    def hoverLeaveEvent(self, ev):
        # logger.debug("ThumbFileItem.hoverLeaveEvent: %s", self.fileinfo)
        self.hovering = False
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

    def reload_thumbnail(self):
        self.reload()

    def on_click_animation(self) -> None:
        if self.animation_timer is not None:
            self.killTimer(self.animation_timer)

        self.animation_timer = self.startTimer(30)
        self.animation_count = 10
        self.update()

    def timerEvent(self, ev) -> None:
        # logger.debug("ThumbFileItem.timerEvent: %s", self.fileinfo)
        if ev.timerId() == self.animation_timer:
            self.animation_count -= 1
            self.update()
            if self.animation_count <= 0:
                self.killTimer(self.animation_timer)
                self.animation_timer = None
        else:
            assert False, "timer foobar"

    def _get_thumbnail(self, flavor: Optional[str]=None) -> Thumbnail:
        if flavor is None:
            flavor = self.thumb_view.flavor

        if flavor == "normal":
            return self.normal_thumbnail
        elif flavor == "large":
            return self.large_thumbnail
        else:
            return self.large_thumbnail


# EOF #
