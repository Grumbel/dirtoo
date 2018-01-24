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


from enum import Enum
import logging
from datetime import datetime

from PyQt5.QtCore import QRectF, QRect
from PyQt5.QtGui import QColor, QFontMetrics, QFont, QPainter, QPainterPath
from PyQt5.QtWidgets import QGraphicsItem

import bytefmt

from dirtools.fileview.file_item import FileItem


def make_scaled_rect(sw, sh, tw, th):
    tratio = tw / th
    sratio = sw / sh

    if tratio > sratio:
        w = sw * th // sh
        h = th
    else:
        w = tw
        h = sh * tw // sw

    return QRect(tw // 2 - w // 2,
                 th // 2 - h // 2,
                 w, h)


def make_unscaled_rect(sw, sh, tw, th):
    return QRect(tw // 2 - sw // 2,
                 th // 2 - sh // 2,
                 sw, sh)


class ThumbnailStatus(Enum):

    ERROR = -1
    NONE = 0
    LOADING = 1
    READY = 2


class ThumbFileItem(FileItem):

    def __init__(self, fileinfo, controller, thumb_view):
        logging.debug("ThumbFileItem.__init__: %s", fileinfo.abspath())
        super().__init__(fileinfo, controller)
        self.setCacheMode(QGraphicsItem.DeviceCoordinateCache)

        self.thumb_view = thumb_view

        self.thumbnail_status = ThumbnailStatus.NONE

        self.hovering = False

        self.pixmap = self.make_shared_pixmap()
        self.have_thumbnail = False

        self.set_tile_size(self.thumb_view.tn_width, self.thumb_view.tn_height)

        self.animation_timer = 0

    def set_tile_size(self, tile_width, tile_height):
        # the size of the base tile
        self.tile_rect = QRectF(0, 0, tile_width, tile_height)

        # the bounding rect for drawing
        self.bounding_rect = QRectF(self.tile_rect.x() - 8,
                                    self.tile_rect.y() - 8,
                                    self.tile_rect.width() + 16,
                                    self.tile_rect.height() + 16)

        # the path used for collision detection
        self.qpainter_path = QPainterPath()
        self.qpainter_path.addRect(0, 0, tile_width, tile_height)

    def paint(self, painter, option, widget):
        logging.debug("ThumbFileItem.paint_items: %s", self.fileinfo.abspath())

        painter.setRenderHint(QPainter.SmoothPixmapTransform)
        painter.setRenderHint(QPainter.TextAntialiasing)
        painter.setRenderHint(QPainter.Antialiasing)

        if self.animation_timer == 0:
            bg_color = QColor(192 + 32, 192 + 32, 192 + 32)
        else:
            bg_color = QColor(192 + 32 - 10 * self.animation_count,
                              192 + 32 - 10 * self.animation_count,
                              192 + 32 - 10 * self.animation_count)

        # background rectangle
        painter.fillRect(-2, -2,
                         self.tile_rect.width() + 4,
                         self.tile_rect.height() + 4,
                         bg_color)

        # hover rectangle
        if self.hovering and self.animation_timer == 0:
            painter.fillRect(-4,
                             -4,
                             self.tile_rect.width() + 8,
                             self.tile_rect.height() + 8,
                             QColor(192, 192, 192))

        self.paint_text_items(painter)
        self.paint_thumbnail(painter)

    def paint_text_items(self, painter):
        # text items
        if self.thumb_view.level_of_detail > 0:
            self.paint_text_item(painter, 0, self.fileinfo.basename())

        if self.thumb_view.level_of_detail > 1:
            self.paint_text_item(painter, 1, self.fileinfo.ext())

        if self.thumb_view.level_of_detail > 2:
            self.paint_text_item(painter, 2, bytefmt.humanize(self.fileinfo.size()))

        if self.thumb_view.level_of_detail > 3:
            dt = datetime.fromtimestamp(self.fileinfo.mtime())
            self.paint_text_item(painter, 3, dt.strftime("%F %T"))

    def paint_text_item(self, painter, row, text):
        font = QFont("Verdana", 8)
        fm = QFontMetrics(font)
        tmp = text
        if fm.width(tmp) > self.tile_rect.width():
            while fm.width(tmp + "…") > self.tile_rect.width():
                tmp = tmp[0:-1]

        if tmp != text:
            text = tmp + "…"

        painter.setFont(font)
        painter.drawText(self.tile_rect.width() / 2 - fm.width(text) / 2,
                         self.tile_rect.height() - 2 + 16 * row - 16 * self.thumb_view.level_of_detail + 14,
                         text)

    def paint_thumbnail(self, painter):
        logging.debug("ThumbFileItem.make_thumbnail: %s", self.fileinfo.abspath())

        if self.pixmap is not None:
            if self.have_thumbnail:
                rect = make_scaled_rect(self.pixmap.width(), self.pixmap.height(),
                                        self.tile_rect.width(), self.tile_rect.width())
            else:
                rect = make_unscaled_rect(self.pixmap.width(), self.pixmap.height(),
                                          self.tile_rect.width(), self.tile_rect.width())
            painter.drawPixmap(rect, self.pixmap)


        if self.thumbnail_status == ThumbnailStatus.NONE:
            self.pixmap = self.thumb_view.shared_pixmaps.image_loading
            self.thumbnail_status = ThumbnailStatus.LOADING
            self.thumb_view.request_thumbnail(self, self.fileinfo, self.thumb_view.flavor)

        if not self.fileinfo.have_access():
            pixmap = self.thumb_view.shared_pixmaps.locked
            painter.setOpacity(0.5)
            painter.drawPixmap(
                self.tile_rect.width() / 2 - pixmap.width() / 2,
                self.tile_rect.height() / 2 - pixmap.height() / 2,
                pixmap)

    def make_shared_pixmap(self):
        logging.debug("ThumbFileItem.make_pixmap: %s", self.fileinfo.abspath())

        # try to load shared pixmaps, this is fast
        pixmap = self.thumb_view.pixmap_from_fileinfo(self.fileinfo, 3 * self.thumb_view.tn_size // 4)
        if pixmap is not None:
            self.thumbnail_status = ThumbnailStatus.READY
            return pixmap
        else:
            return self.thumb_view.shared_pixmaps.image_loading

    def hoverEnterEvent(self, ev):
        self.hovering = True
        self.controller.show_current_filename(self.fileinfo.filename())
        self.update()

    def hoverLeaveEvent(self, ev):
        self.hovering = False
        self.controller.show_current_filename("")
        self.update()

    def set_thumbnail_pixmap(self, pixmap, flavor):
        self.pixmap = pixmap

        if self.pixmap is None:
            self.pixmap = self.thumb_view.shared_pixmaps.image_missing
            self.thumbnail_status == ThumbnailStatus.ERROR
            self.have_thumbnail = False
        else:
            self.have_thumbnail = True

        self.update()

    def boundingRect(self):
        return self.bounding_rect

    def shape(self):
        return self.qpainter_path

    def reload(self):
        self.thumbnail_status = ThumbnailStatus.NONE
        self.pixmap = self.make_shared_pixmap()
        self.have_thumbnail = False
        self.update()

    def reload_thumbnail(self):
        self.reload(x=self.pos().x(), y=self.pos().y())

    def on_click_animation(self):
        self.animation_timer = self.startTimer(30)
        self.animation_count = 10
        self.update()

    def timerEvent(self, ev):
        if ev.timerId() == self.animation_timer:
            self.animation_count -= 1
            self.update()
            if self.animation_count > 0:
                self.animation_timer = self.startTimer(30)
            else:
                self.animation_timer = 0


# EOF #
