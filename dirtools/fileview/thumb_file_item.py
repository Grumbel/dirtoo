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


from typing import Union, Dict, Any

from enum import Enum
import logging
from datetime import datetime

from PyQt5.QtCore import Qt, QRectF, QRect
from PyQt5.QtGui import QColor, QFontMetrics, QFont, QPainter, QPainterPath
from PyQt5.QtWidgets import QGraphicsItem

import bytefmt

from dirtools.fileview.file_item import FileItem
from dirtools.mediainfo import split_duration


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

        if not self.parent_item.fileinfo.is_thumbnailable():
            self.status = ThumbnailStatus.THUMBNAIL_UNAVAILABLE
        else:
            self.status = ThumbnailStatus.LOADING
            self.parent_item.thumb_view.request_thumbnail(
                self.parent_item, self.parent_item.fileinfo, self.flavor)


class ThumbFileItem(FileItem):

    def __init__(self, fileinfo, controller, thumb_view):
        logging.debug("ThumbFileItem.__init__: %s", fileinfo.abspath())
        super().__init__(fileinfo, controller)
        self.setCacheMode(QGraphicsItem.DeviceCoordinateCache)

        self.thumb_view = thumb_view

        self.hovering: bool = False

        self.icon = self.make_icon()
        self.normal_thumbnail: Thumbnail = Thumbnail("normal", self)
        self.large_thumbnail: Thumbnail = Thumbnail("large", self)
        self.metadata: Union[Dict[str, Any], None] = None

        self.set_tile_size(self.thumb_view.tn_width, self.thumb_view.tn_height)

        self.animation_timer: Union[int, None] = None

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
        logging.debug("ThumbFileItem.paint: %s", self.fileinfo.abspath())

        if self.metadata is None:
            self.controller.request_metadata(self.fileinfo)
            self.metadata = {}

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
        if not self.thumb_view.column_style or self.animation_timer is not None:
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
                             QColor(192, 192, 192))

        self.paint_text_items(painter)
        self.paint_thumbnail(painter)

        if self.hovering and self.animation_timer is None:
            painter.setCompositionMode(QPainter.CompositionMode_Overlay)
            painter.setOpacity(0.75)
            self.paint_thumbnail(painter)
            painter.setOpacity(1.0)
            painter.setCompositionMode(QPainter.CompositionMode_SourceOver)

        if self.thumb_view.level_of_detail > 1:
            self.paint_metadata(painter)
            self.paint_overlay(painter)

    def paint_text_items(self, painter):
        # text items
        if self.thumb_view.level_of_detail > 0:
            self.paint_text_item(painter, 0, self.fileinfo.basename())

        if self.thumb_view.level_of_detail > 2:
            painter.setPen(QColor(96, 96, 96))
            self.paint_text_item(painter, 1, bytefmt.humanize(self.fileinfo.size()))

        if self.thumb_view.level_of_detail > 3:
            dt = datetime.fromtimestamp(self.fileinfo.mtime())
            painter.setPen(QColor(96, 96, 96))
            self.paint_text_item(painter, 2, dt.strftime("%F %T"))

    def paint_text_item(self, painter, row, text):
        font = QFont("Verdana", 8)
        fm = QFontMetrics(font)
        tmp = text

        if not self.thumb_view.column_style:
            target_width = self.tile_rect.width()
        else:
            target_width = self.tile_rect.width() - 32 - 4

        if fm.width(tmp) > target_width:
            while fm.width(tmp + "…") > target_width:
                tmp = tmp[0:-1]

        if tmp != text:
            text = tmp + "…"

        painter.setFont(font)

        k = [0, 1, 1, 2, 3][self.thumb_view.level_of_detail]
        if not self.thumb_view.column_style:
            painter.drawText(self.tile_rect.width() / 2 - fm.width(text) / 2,
                             self.tile_rect.height() - 2 + 16 * row - 16 * k + 14,
                             text)
        else:
            painter.drawText(self.tile_rect.height() + 4,
                             self.tile_rect.height() - 2 + 16 * row - 16 * k + 14,
                             text)

    def paint_thumbnail(self, painter):
        logging.debug("ThumbFileItem.paint_thumbnail: %s", self.fileinfo.abspath())

        if self.thumb_view.column_style:
            self.paint_icon(painter, self.icon)
            return

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

            if self.fileinfo.is_video():
                painter.setOpacity(0.5)
                # painter.setCompositionMode(QPainter.CompositionMode_Plus)
                painter.drawPixmap(QRect(self.thumbnail_rect.width() - 24 - 2,
                                         self.thumbnail_rect.height() - 24 - 2,
                                         24, 24),
                                   self.thumb_view.shared_pixmaps.video)
                painter.setOpacity(1.0)
            elif self.fileinfo.is_image():
                painter.setOpacity(0.5)
                # painter.setCompositionMode(QPainter.CompositionMode_Plus)
                painter.drawPixmap(QRect(self.thumbnail_rect.width() - 24 - 2,
                                         self.thumbnail_rect.height() - 24 - 2,
                                         24, 24),
                                   self.thumb_view.shared_pixmaps.image)
                painter.setOpacity(1.0)

    def paint_metadata(self, painter):
        metadata = self.fileinfo.metadata()

        font = QFont("Verdana", 8)
        fm = QFontMetrics(font)
        painter.setFont(font)

        top_left_text = ""
        top_right_text = ""

        if "type" in self.fileinfo.metadata() and self.fileinfo.metadata()["type"] == "error":
            painter.setOpacity(0.5)
            # painter.setCompositionMode(QPainter.CompositionMode_Plus)
            painter.drawPixmap(QRect(2, 2, 24, 24),
                               self.thumb_view.shared_pixmaps.error)
            painter.setOpacity(1.0)

        if 'duration' in metadata:
            hours, minutes, seconds = split_duration(metadata['duration'])
            top_left_text = "{:d}:{:02d}:{:02d}".format(hours, minutes, seconds)

        if 'pages' in metadata:
            top_left_text = "{:d} pages".format(metadata['pages'])

        if 'file_count' in metadata:
            top_left_text = "{:d} files".format(metadata['file_count'])

        if 'framerate' in metadata:
            top_right_text = "{:g}fps".format(metadata['framerate'])

        if top_left_text:
            painter.setPen(Qt.NoPen)
            painter.setBrush(QColor(255, 255, 255, 160))
            painter.drawRect(0, 0, fm.width(top_left_text) + 4, 16)
            painter.setPen(QColor(0, 0, 0))
            painter.drawText(2, 12, top_left_text)

        if top_right_text:
            w = fm.width(top_right_text)
            painter.setPen(Qt.NoPen)
            painter.setBrush(QColor(255, 255, 255, 160))
            painter.drawRect(self.thumbnail_rect.width() - w, 0, w + 4, 16)
            painter.setPen(QColor(0, 0, 0))
            painter.drawText(self.thumbnail_rect.width() - w, 12, top_right_text)

        if 'width' in metadata and 'height' in metadata:
            text = "{}x{}".format(metadata['width'], metadata['height'])

            painter.setPen(Qt.NoPen)
            painter.setBrush(QColor(255, 255, 255, 160))
            painter.drawRect(0, self.thumbnail_rect.height() - 16, fm.width(text) + 4, 16)
            painter.setPen(QColor(0, 0, 0))
            painter.drawText(2, self.thumbnail_rect.height() - 4, text)

    def paint_overlay(self, painter):
        if self.thumb_view.column_style:
            return

        if self.fileinfo.have_access() is False:
            painter.setOpacity(0.5)
            self.paint_icon(painter, self.thumb_view.shared_icons.locked)

        thumbnail = self._get_thumbnail()
        if thumbnail.status == ThumbnailStatus.LOADING or thumbnail.status == ThumbnailStatus.INITIAL:
            painter.setOpacity(0.5)
            painter.drawPixmap(QRect(self.thumbnail_rect.width() - 32,
                                     2,
                                     32, 32),
                               self.thumb_view.shared_pixmaps.loading)
            painter.setOpacity(1.0)
        elif thumbnail.status == ThumbnailStatus.THUMBNAIL_ERROR:
            self.paint_icon(painter, self.thumb_view.shared_icons.image_error)

    def paint_tiny_icon(self, painter, icon):
        painter.setOpacity(0.5)
        icon.paint(painter, QRect(self.tile_rect.width() - 48, 0, 48, 48))

    def paint_icon(self, painter, icon):
        if not self.thumb_view.column_style:
            rect = make_unscaled_rect(self.tile_rect.width() * 3 // 4, self.tile_rect.width() * 3 // 4,
                                      self.tile_rect.width(), self.tile_rect.width())
        else:
            rect = QRect(0, 0, self.tile_rect.height(), self.tile_rect.height())

        icon.paint(painter, rect)

    def make_icon(self):
        logging.debug("ThumbFileItem.make_pixmap: %s", self.fileinfo.abspath())
        icon = self.thumb_view.icon_from_fileinfo(self.fileinfo)
        return icon

    def hoverEnterEvent(self, ev):
        self.hovering = True
        self.controller.show_current_filename(self.fileinfo.abspath())
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
                self.animation_timer = None
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
