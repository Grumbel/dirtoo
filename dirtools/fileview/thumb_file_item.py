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
from datetime import datetime
from enum import Enum

from PyQt5.QtCore import Qt, QRectF, QRect, QMargins
from PyQt5.QtGui import QColor, QPainter, QPainterPath, QPixmap, QIcon, QImage

import bytefmt

from dirtools.fileview.file_item import FileItem
from dirtools.fileview.file_info import FileInfo
from dirtools.mediainfo import split_duration

logger = logging.getLogger(__name__)


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

    def __init__(self, flavor: str, parent_item) -> None:
        self.parent_item = parent_item
        self.pixmap: Optional[QPixmap] = None
        self.status: ThumbnailStatus = ThumbnailStatus.INITIAL
        self.flavor: str = flavor
        self.mtime: float = 0

    def get_pixmap(self) -> QPixmap:
        if self.status == ThumbnailStatus.THUMBNAIL_READY:
            assert self.pixmap is not None
            return self.pixmap
        else:
            return None

    def set_thumbnail_image(self, image: QImage) -> None:
        if image is None:
            self.status = ThumbnailStatus.THUMBNAIL_UNAVAILABLE
            self.pixmap = None
        else:
            assert not image.isNull()
            self.status = ThumbnailStatus.THUMBNAIL_READY
            self.pixmap = QPixmap(image)
            try:
                mtime_txt = image.text("Thumb::MTime")
                self.mtime = int(mtime_txt)

                if int(self.parent_item.fileinfo.mtime()) != self.mtime:
                    self.reset()
            except ValueError as err:
                logger.error("%s: couldn't read Thumb::MTime tag on thumbnail: %s",
                             self.parent_item.fileinfo.location(), err)

    def reset(self) -> None:
        self.pixmap = None
        self.status = ThumbnailStatus.INITIAL
        self.mtime = 0

        self.request(force=True)

    def request(self, force=False) -> None:
        assert self.status != ThumbnailStatus.LOADING

        if not self.parent_item.fileinfo.is_thumbnailable():
            self.status = ThumbnailStatus.THUMBNAIL_UNAVAILABLE
        else:
            self.status = ThumbnailStatus.LOADING
            self.parent_item.thumb_view.request_thumbnail(
                self.parent_item, self.parent_item.fileinfo, self.flavor, force=force)


class ThumbFileItem(FileItem):

    def __init__(self, fileinfo: FileInfo, controller, final, thumb_view) -> None:
        logger.debug("ThumbFileItem.__init__: %s", fileinfo)
        super().__init__(fileinfo, controller)
        # self.setCacheMode(QGraphicsItem.DeviceCoordinateCache)

        self.thumb_view = thumb_view

        self.new = False
        self.hovering: bool = False
        self.animation_count = 0

        self.icon = self.make_icon()
        self.normal_thumbnail: Thumbnail = Thumbnail("normal", self)
        self.large_thumbnail: Thumbnail = Thumbnail("large", self)
        self.metadata: Optional[Dict[str, Any]] = None

        self.set_tile_size(self.thumb_view._tile_style.tile_width, self.thumb_view._tile_style.tile_height)
        self.animation_timer: Optional[int] = None

        self._file_is_final = final

    def set_fileinfo(self, fileinfo: FileInfo, final=False):
        self.fileinfo = fileinfo
        self._file_is_final = final
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
                bg_color = QColor(192 + 32, 192 + 32, 192 + 32)
            elif self.fileinfo.uid() == 0:
                bg_color = QColor(192 + 32, 176, 176)
            else:
                bg_color = QColor(176, 192 + 32, 176)
        else:
            bg_color = QColor(192 + 32 - 10 * self.animation_count,
                              192 + 32 - 10 * self.animation_count,
                              192 + 32 - 10 * self.animation_count)

        # background rectangle
        if not self.thumb_view._column_style or self.animation_timer is not None:
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


class ThumbFileItemRenderer:

    def __init__(self, item):
        self.fileinfo = item.fileinfo
        self.icon = item.icon
        self.thumbnail = item._get_thumbnail()
        self.level_of_detail = item.thumb_view._level_of_detail
        self.style = item.thumb_view._style
        self.column_style = False  # self.thumb_view.column_style
        self.tile_rect = item.tile_rect
        self.thumbnail_rect = QRect(0, 0, item.tile_rect.width(), item.tile_rect.width())
        self.hovering = item.hovering
        self.animation_timer = item.animation_timer
        self.new = False
        self.crop_thumbnails = item.thumb_view._crop_thumbnails
        self.is_selected = item.isSelected()
        self.is_cursor = item.thumb_view._cursor_item == item

        self.cache_pixmap = None

    def render(self, painter: QPainter) -> None:
        if True:
            self.paint(painter)
        else:
            if self.cache_pixmap is None:
                self.cache_pixmap = QPixmap(self.tile_rect.width(), self.tile_rect.height())
                self.cache_pixmap.fill()
                pixmap_painter = QPainter(self.cache_pixmap)
                self.paint(pixmap_painter)
                pixmap_painter.end()

            painter.drawPixmap(self.tile_rect.x(), self.tile_rect.y(),
                               self.cache_pixmap)

    def paint(self, painter: QPainter) -> None:
        self.paint_text_items(painter)
        self.paint_thumbnail(painter)

        if self.hovering and self.animation_timer is None:
            painter.setCompositionMode(QPainter.CompositionMode_Overlay)
            painter.setOpacity(0.75)
            self.paint_thumbnail(painter)
            painter.setOpacity(1.0)
            painter.setCompositionMode(QPainter.CompositionMode_SourceOver)

        if self.level_of_detail > 1:
            self.paint_metadata(painter)
            self.paint_overlay(painter)

        if self.is_selected:
            painter.save()
            painter.setCompositionMode(QPainter.CompositionMode_SourceOver)
            painter.setOpacity(0.5)
            painter.fillRect(self.tile_rect, QColor(127, 192, 255))
            painter.setCompositionMode(QPainter.CompositionMode_SourceOver)
            painter.setOpacity(1.0)
            painter.setPen(QColor(96, 127, 255))
            painter.setBrush(Qt.NoBrush)
            painter.drawRect(self.tile_rect)
            painter.restore()

        if self.is_cursor:
            painter.setOpacity(1.0)
            painter.setPen(QColor(0, 0, 0))
            painter.setBrush(QColor(255, 255, 255, 96))
            painter.drawRect(self.tile_rect)

    def paint_text_items(self, painter: QPainter) -> None:
        # text items
        if self.level_of_detail > 0:
            self.paint_text_item(painter, 0, self.fileinfo.basename())

        if self.level_of_detail > 2:
            painter.setPen(QColor(96, 96, 96))
            self.paint_text_item(painter, 1, bytefmt.humanize(self.fileinfo.size()))

        if self.level_of_detail > 3:
            dt = datetime.fromtimestamp(self.fileinfo.mtime())
            painter.setPen(QColor(96, 96, 96))
            self.paint_text_item(painter, 2, dt.strftime("%F %T"))

    def paint_text_item(self, painter: QPainter, row: int, text: str) -> None:
        font = self.style.font
        fm = self.style.fm

        tmp = text

        if not self.column_style:
            target_width = self.tile_rect.width()
        else:
            target_width = self.tile_rect.width() - 32 - 4

        if fm.width(tmp) > target_width:
            while fm.width(tmp + "…") > target_width:
                tmp = tmp[0:-1]

        if tmp != text:
            text = tmp + "…"

        painter.setFont(font)

        k = [0, 1, 1, 2, 3][self.level_of_detail]
        if not self.column_style:
            painter.drawText(self.tile_rect.width() / 2 - fm.width(text) / 2,
                             self.tile_rect.height() - 2 + 16 * row - 16 * k + 14,
                             text)
        else:
            painter.drawText(self.tile_rect.height() + 4,
                             self.tile_rect.height() - 2 + 16 * row - 16 * k + 14,
                             text)

    def paint_thumbnail(self, painter: QPainter) -> None:

        if self.column_style:
            self.paint_icon(painter, self.icon)
            return

        thumbnail = self.thumbnail

        if thumbnail.status == ThumbnailStatus.INITIAL:
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

            if not self.crop_thumbnails:
                rect = make_scaled_rect(pixmap.width(), pixmap.height(),
                                        self.thumbnail_rect.width(), self.thumbnail_rect.width())
                painter.drawPixmap(rect, pixmap)
            else:
                srcrect = make_cropped_rect(pixmap.width(), pixmap.height(),
                                            self.thumbnail_rect.width(), self.thumbnail_rect.width())
                painter.drawPixmap(self.thumbnail_rect, pixmap, srcrect)

    def paint_metadata(self, painter: QPainter) -> None:
        metadata = self.fileinfo.metadata()

        font = self.style.font
        fm = self.style.fm
        painter.setFont(font)

        top_left_text = ""
        top_right_text = ""
        bottom_left_text = ""

        if self.new:
            painter.drawPixmap(QRect(2, 2, 24, 24),
                               self.style.shared_pixmaps.new)

        if "type" in self.fileinfo.metadata() and self.fileinfo.metadata()["type"] == "error":
            painter.drawPixmap(QRect(2, 2, 24, 24),
                               self.style.shared_pixmaps.error)

        if 'duration' in metadata:
            hours, minutes, seconds = split_duration(metadata['duration'])
            top_left_text = "{:d}:{:02d}:{:02d}".format(hours, minutes, seconds)

        if 'pages' in metadata:
            top_left_text = "{:d} pages".format(metadata['pages'])

        if 'file_count' in metadata:
            top_left_text = "{:d} files".format(metadata['file_count'])

        if 'framerate' in metadata:
            top_right_text = "{:g}fps".format(metadata['framerate'])

        if 'width' in metadata and 'height' in metadata:
            bottom_left_text = "{}x{}".format(metadata['width'], metadata['height'])

        if top_left_text:
            w = fm.width(top_left_text)
            painter.setPen(Qt.NoPen)
            painter.setBrush(QColor(255, 255, 255, 160))
            painter.drawRect(0, 0, w + 4, 16)
            painter.setPen(QColor(0, 0, 0))
            painter.drawText(2, 12, top_left_text)

        if top_right_text:
            w = fm.width(top_right_text)
            painter.setPen(Qt.NoPen)
            painter.setBrush(QColor(255, 255, 255, 160))
            painter.drawRect(self.thumbnail_rect.width() - w - 4, 0, w + 4, 16)
            painter.setPen(QColor(0, 0, 0))
            painter.drawText(self.thumbnail_rect.width() - w - 2, 12, top_right_text)

        if bottom_left_text:
            w = fm.width(bottom_left_text)
            painter.setPen(Qt.NoPen)
            painter.setBrush(QColor(255, 255, 255, 160))
            painter.drawRect(0, self.thumbnail_rect.height() - 16, w + 4, 16)
            painter.setPen(QColor(0, 0, 0))
            painter.drawText(2, self.thumbnail_rect.height() - 4, bottom_left_text)

        if self.fileinfo.is_video():
            painter.setOpacity(0.5)
            # painter.setCompositionMode(QPainter.CompositionMode_Plus)
            painter.drawPixmap(QRect(self.thumbnail_rect.width() - 24 - 2,
                                     self.thumbnail_rect.height() - 24 - 2,
                                     24, 24),
                               self.style.shared_pixmaps.video)
            painter.setOpacity(1.0)
        elif self.fileinfo.is_image():
            painter.setOpacity(0.5)
            # painter.setCompositionMode(QPainter.CompositionMode_Plus)
            painter.drawPixmap(QRect(self.thumbnail_rect.width() - 24 - 2,
                                     self.thumbnail_rect.height() - 24 - 2,
                                     24, 24),
                               self.style.shared_pixmaps.image)
            painter.setOpacity(1.0)

    def paint_overlay(self, painter: QPainter) -> None:
        if self.column_style:
            return

        if self.fileinfo.have_access() is False:
            painter.setOpacity(0.5)
            m = int(self.thumbnail_rect.width() * 0.125)
            painter.drawPixmap(self.thumbnail_rect.marginsRemoved(QMargins(m, m, m, m)),
                               self.style.shared_pixmaps.locked)
            painter.setOpacity(1.0)

        thumbnail = self.thumbnail
        if thumbnail.status == ThumbnailStatus.LOADING or thumbnail.status == ThumbnailStatus.INITIAL:
            painter.setOpacity(0.5)
            painter.drawPixmap(QRect(self.thumbnail_rect.width() - 32,
                                     2,
                                     32, 32),
                               self.style.shared_pixmaps.loading)
            painter.setOpacity(1.0)
        elif thumbnail.status == ThumbnailStatus.THUMBNAIL_ERROR:
            self.paint_icon(painter, self.style.shared_icons.image_error)

    def paint_tiny_icon(self, painter: QPainter, icon: QIcon) -> None:
        painter.setOpacity(0.5)
        icon.paint(painter, QRect(self.tile_rect.width() - 48, 0, 48, 48))

    def paint_icon(self, painter: QPainter, icon: QIcon) -> None:
        if not self.column_style:
            rect = make_unscaled_rect(self.tile_rect.width() * 3 // 4, self.tile_rect.width() * 3 // 4,
                                      self.tile_rect.width(), self.tile_rect.width())
        else:
            rect = QRect(0, 0, self.tile_rect.height(), self.tile_rect.height())

        icon.paint(painter, rect)


# EOF #
