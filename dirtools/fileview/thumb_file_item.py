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


def make_cropped_rect(sw, sh, tw, th):
    """Calculate a srcrect that fits into the dstrect and fills it,
    cropping the src."""

    tratio = tw / th
    sratio = sw / sh

    if tratio > sratio:
        w = sw
        h = th * sw // tw

        return QRect(0,
                     0,  # top align
                     # max(0, sh // 3 - h // 2),
                     # center: sh // 2 - h // 2,
                     w, h)
    else:
        w = tw * sh // tw
        h = sh

        return QRect(sw // 2 - w // 2,
                     0,
                     w, h)


class ThumbnailStatus(Enum):

    INITIAL = 0
    LOADING = 1
    THUMBNAIL_ERROR = 2
    THUMBNAIL_UNAVAILABLE = 3
    THUMBNAIL_READY = 4


class Thumbnail:

    def __init__(self, flavor, parent_item):
        self.parent_item = parent_item
        self.pixmap = None
        self.status = ThumbnailStatus.INITIAL
        self.flavor = flavor

    def get_pixmap(self):
        if self.status == ThumbnailStatus.THUMBNAIL_READY:
            assert self.pixmap is not None
            return self.pixmap
        else:
            return None

    def set_thumbnail_pixmap(self, pixmap):
        if pixmap is None:
            self.status = ThumbnailStatus.THUMBNAIL_UNAVAILABLE
            self.pixmap = None
        else:
            assert not pixmap.isNull()
            self.status = ThumbnailStatus.THUMBNAIL_READY
            self.pixmap = pixmap

    def request(self):
        assert self.status != ThumbnailStatus.LOADING

        self.status = ThumbnailStatus.LOADING
        self.parent_item.thumb_view.request_thumbnail(
            self.parent_item, self.parent_item.fileinfo, self.flavor)


class ThumbFileItem(FileItem):

    def __init__(self, fileinfo, controller, thumb_view):
        logging.debug("ThumbFileItem.__init__: %s", fileinfo.abspath())
        super().__init__(fileinfo, controller)
        self.setCacheMode(QGraphicsItem.DeviceCoordinateCache)

        self.thumb_view = thumb_view

        self.hovering = False

        self.icon = self.make_icon()
        self.normal_thumbnail = Thumbnail("normal", self)
        self.large_thumbnail = Thumbnail("large", self)

        self.set_tile_size(self.thumb_view.tn_width, self.thumb_view.tn_height)

        self.animation_timer = None

    def set_tile_size(self, tile_width, tile_height):
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

    def paint(self, painter, option, widget):
        logging.debug("ThumbFileItem.paint_items: %s", self.fileinfo.abspath())

        painter.setRenderHint(QPainter.SmoothPixmapTransform)
        painter.setRenderHint(QPainter.TextAntialiasing)
        painter.setRenderHint(QPainter.Antialiasing)

        if self.animation_timer is None:
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
        if self.hovering and self.animation_timer is None:
            painter.fillRect(-4,
                             -4,
                             self.tile_rect.width() + 8,
                             self.tile_rect.height() + 8,
                             QColor(192, 192, 192))

        self.paint_text_items(painter)
        self.paint_thumbnail(painter)
        self.paint_overlay(painter)

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

        thumbnail = self._get_thumbnail()

        if thumbnail.status == ThumbnailStatus.INITIAL:
            thumbnail.request()
            self.paint_icon(painter, self.icon)

        elif thumbnail.status == ThumbnailStatus.LOADING:
            self.paint_icon(painter, self.icon)

        elif thumbnail.status == ThumbnailStatus.THUMBNAIL_UNAVAILABLE:
            self.paint_icon(painter, self.icon)

        elif thumbnail.status == ThumbnailStatus.THUMBNAIL_ERROR:
            self.paint_icon(painter, self.icon)

        elif thumbnail.status == ThumbnailStatus.THUMBNAIL_READY:
            pixmap = thumbnail.get_pixmap()
            assert pixmap is not None

            if not self.thumb_view.crop_thumbnails:
                rect = make_scaled_rect(pixmap.width(), pixmap.height(),
                                        self.thumbnail_rect.width(), self.thumbnail_rect.width())
                painter.drawPixmap(rect, pixmap)
            else:
                srcrect = make_cropped_rect(pixmap.width(), pixmap.height(),
                                            self.thumbnail_rect.width(), self.thumbnail_rect.width())
                painter.drawPixmap(self.thumbnail_rect, pixmap, srcrect)

            self.paint_tiny_icon(painter, self.icon)

    def paint_overlay(self, painter):
        if self.fileinfo.have_access() == False:
            painter.setOpacity(0.5)
            self.paint_icon(painter, self.thumb_view.shared_pixmaps.locked)

        thumbnail = self._get_thumbnail()
        if thumbnail.status == ThumbnailStatus.LOADING or thumbnail.status == ThumbnailStatus.INITIAL:
            self.paint_icon(painter, self.thumb_view.shared_pixmaps.image_loading)
        elif thumbnail.status == ThumbnailStatus.THUMBNAIL_ERROR:
            self.paint_icon(painter, self.thumb_view.shared_pixmaps.image_error)

    def paint_tiny_icon(self, painter, icon):
        painter.setOpacity(0.75)
        icon.paint(painter, QRect(self.tile_rect.width() - 48, 0, 48, 48))

    def paint_icon(self, painter, icon):
        rect = make_unscaled_rect(self.tile_rect.width() * 3 // 4, self.tile_rect.width() * 3 // 4,
                                  self.tile_rect.width(), self.tile_rect.width())
        icon.paint(painter, rect)

    def make_icon(self):
        logging.debug("ThumbFileItem.make_pixmap: %s", self.fileinfo.abspath())
        icon = self.thumb_view.icon_from_fileinfo(self.fileinfo)
        return icon

    def hoverEnterEvent(self, ev):
        self.hovering = True
        self.controller.show_current_filename(self.fileinfo.filename())
        self.update()

    def hoverLeaveEvent(self, ev):
        self.hovering = False
        self.controller.show_current_filename("")
        self.update()

    def set_thumbnail_pixmap(self, pixmap, flavor):
        thumbnail = self._get_thumbnail(flavor)
        thumbnail.set_thumbnail_pixmap(pixmap)
        self.update()

    def set_icon(self, icon):
        self.icon = icon

    def boundingRect(self):
        return QRectF(self.bounding_rect)

    def shape(self):
        return self.qpainter_path

    def reload(self):
        self.normal_thumbnail = Thumbnail("normal", self)
        self.large_thumbnail = Thumbnail("large", self)
        self.update()

    def reload_thumbnail(self):
        self.reload()

    def on_click_animation(self):
        if self.animation_timer is not None:
            self.killTimer(self.animation_timer)

        self.animation_timer = self.startTimer(30)
        self.animation_count = 10
        self.update()

    def timerEvent(self, ev):
        if ev.timerId() == self.animation_timer:
            self.animation_count -= 1
            self.update()
            if self.animation_count <= 0:
                self.killTimer(self.animation_timer)
        else:
            assert False, "timer foobar"

    def _get_thumbnail(self, flavor=None):
        if flavor is None:
            flavor = self.thumb_view.flavor

        if flavor == "normal":
            return self.normal_thumbnail
        elif flavor == "large":
            return self.large_thumbnail
        else:
            return self.large_thumbnail


# EOF #
