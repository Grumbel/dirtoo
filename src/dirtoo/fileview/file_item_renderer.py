# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2018 Ingo Ruhnke <grumbel@gmail.com>
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


from typing import TYPE_CHECKING, Tuple

import logging
from datetime import datetime

from PyQt6.QtCore import Qt, QRect, QRectF, QPoint, QPointF, QMargins
from PyQt6.QtGui import QColor, QPainter, QIcon, QTextOption, QBrush

import bytefmt

from dirtoo.thumbnail.thumbnail import ThumbnailStatus
from dirtoo.mediainfo import split_duration
from dirtoo.fileview.mode import FileItemStyle
from dirtoo.fileview.scaler import make_unscaled_rect, make_scaled_rect, make_cropped_rect

if TYPE_CHECKING:
    from dirtoo.fileview.file_item import FileItem


logger = logging.getLogger(__name__)


class FileItemRenderer:

    def __init__(self, item: 'FileItem') -> None:
        self.fileinfo = item.fileinfo
        self.icon = item.icon
        self.thumbnail = item._get_thumbnail()

        self._item_style = item.file_view._mode._item_style
        self.level_of_detail = item.file_view._mode._level_of_detail
        self.style = item.file_view._style
        self.zoom_index = item.file_view._mode._zoom_index

        self.tile_rect = item.tile_rect
        self.hovering = item.hovering
        self.animation_timer = item.animation_timer
        self.new = item._new
        self.crop_thumbnails = item.file_view._crop_thumbnails
        self.is_selected = item.isSelected()
        self.is_cursor = item.file_view._cursor_item == item

        self.thumbnail_rect: QRect = QRect(0, 0, item.tile_rect.width(), item.tile_rect.width())

    def render(self, painter: QPainter) -> None:
        if self._item_style == FileItemStyle.SMALLICON:
            self.paint_smallicon_view(painter)
        elif self._item_style == FileItemStyle.DETAIL:
            self.paint_detail_view(painter)
        else:
            self.paint(painter)

        if self.is_selected:
            painter.save()
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)
            painter.setOpacity(0.5)
            painter.fillRect(self.tile_rect, QColor(127, 192, 255))
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)
            painter.setOpacity(1.0)
            painter.setPen(QColor(96, 127, 255))
            painter.setBrush(Qt.BrushStyle.NoBrush)
            painter.drawRect(self.tile_rect)
            painter.restore()

        if self.is_cursor:
            painter.setOpacity(1.0)
            painter.setPen(QColor(0, 0, 0))
            painter.setBrush(QColor(255, 255, 255, 96))
            painter.drawRect(self.tile_rect)

    def paint_smallicon_view(self, painter: QPainter) -> None:
        self.thumbnail_rect = QRect(0, 0, self.tile_rect.height(), self.tile_rect.height())

        font = self.style.font
        fm = self.style.fm

        painter.setFont(font)
        self.paint_thumbnail(painter)

        if self.zoom_index in [0, 1]:
            text_option = QTextOption(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter)
            text_option.setWrapMode(QTextOption.WrapMode.NoWrap)
            text_rect = QRect(QPoint(int(self.tile_rect.height()) + 4,
                                     0),
                              QPoint(self.tile_rect.width(),
                                     self.tile_rect.height()))
            text = self.fileinfo.basename()
            text = fm.elidedText(text, Qt.TextElideMode.ElideRight, text_rect.width())
            painter.drawText(QRectF(text_rect),
                             text,
                             text_option)
        elif self.zoom_index in [2]:
            text_rect = QRect(QPoint(self.tile_rect.height() + 4,
                                     0),
                              QPoint(self.tile_rect.width() - 80,
                                     self.tile_rect.height()))
            text = self.fileinfo.basename()
            text = fm.elidedText(text, Qt.TextElideMode.ElideRight, text_rect.width())
            text_option = QTextOption(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter)
            text_option.setWrapMode(QTextOption.WrapMode.NoWrap)
            painter.drawText(QRectF(text_rect), text, text_option)

            text_rect = QRect(QPoint(self.tile_rect.width() - 80,
                                     0),
                              QPoint(self.tile_rect.width(),
                                     self.tile_rect.height()))
            text = bytefmt.humanize(self.fileinfo.size())
            text = fm.elidedText(text, Qt.TextElideMode.ElideRight, text_rect.width())
            text_option = QTextOption(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)

            text_option.setWrapMode(QTextOption.WrapMode.NoWrap)
            painter.setPen(QColor(96, 96, 96))
            painter.drawText(QRectF(text_rect), text, text_option)
        else:
            top_left_text, top_right_text, bottom_left_text, bottom_right = self.make_text()

            row1_rect = QRect(QPoint(self.tile_rect.left() + self.tile_rect.height() + 8,
                                     self.tile_rect.top()),
                              QPoint(self.tile_rect.right() - 8,
                                     self.tile_rect.top() + self.tile_rect.height() // 2))
            row2_rect = QRect(QPoint(self.tile_rect.left() + self.tile_rect.height() + 8,
                                     self.tile_rect.top() + self.tile_rect.height() // 2),
                              QPoint(self.tile_rect.right() - 8,
                                     self.tile_rect.bottom()))

            # row 1
            text_rect = QRect(QPoint(row1_rect.left(),
                                     row1_rect.top()),
                              QPoint(row1_rect.right() - 80,
                                     row1_rect.bottom()))
            text = self.fileinfo.basename()
            text = fm.elidedText(text, Qt.TextElideMode.ElideRight, text_rect.width())
            text_option = QTextOption(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter)
            text_option.setWrapMode(QTextOption.WrapMode.NoWrap)
            painter.drawText(QRectF(text_rect), text, text_option)

            text_rect = QRect(QPoint(row1_rect.left() - 80,
                                     row1_rect.top()),
                              QPoint(row1_rect.right(),
                                     row1_rect.bottom()))
            text = bytefmt.humanize(self.fileinfo.size())
            text = fm.elidedText(text, Qt.TextElideMode.ElideRight, text_rect.width())
            text_option = QTextOption(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)

            text_option.setWrapMode(QTextOption.WrapMode.NoWrap)
            painter.setPen(QColor(96, 96, 96))
            painter.drawText(QRectF(text_rect), text, text_option)

            # row 2
            text_rect = QRect(QPoint(row2_rect.left(),
                                     row2_rect.top()),
                              QPoint(row2_rect.right() - 80,
                                     row2_rect.bottom()))
            text = bottom_left_text
            text = fm.elidedText(text, Qt.TextElideMode.ElideRight, text_rect.width())
            text_option = QTextOption(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter)
            text_option.setWrapMode(QTextOption.WrapMode.NoWrap)
            painter.drawText(QRectF(text_rect), text, text_option)

            text_rect = QRect(QPoint(row2_rect.left() - 80,
                                     row2_rect.top()),
                              QPoint(row2_rect.right(),
                                     row2_rect.bottom()))
            text = top_left_text
            text = fm.elidedText(text, Qt.TextElideMode.ElideRight, text_rect.width())
            text_option = QTextOption(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)

            text_option.setWrapMode(QTextOption.WrapMode.NoWrap)
            painter.setPen(QColor(96, 96, 96))
            painter.drawText(QRectF(text_rect), text, text_option)

    def paint_detail_view(self, painter: QPainter) -> None:
        self.thumbnail_rect = QRect(0, 0, self.tile_rect.height(), self.tile_rect.height())

        self.paint_thumbnail(painter)

        lst = ([self.fileinfo.basename(), bytefmt.humanize(self.fileinfo.size())] +
               list(self.make_text()))

        text_option = QTextOption(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter)
        text_option.setWrapMode(QTextOption.WrapMode.NoWrap)

        total_width = self.tile_rect.width() - self.thumbnail_rect.width() - 8

        widths = [60, 10, 10, 10, 10, 10]
        x = self.thumbnail_rect.width() + 4
        for idx, text in enumerate(lst[:-1]):
            if idx != 0:
                text_option.setAlignment(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)

            width = total_width * (widths[idx] // 100)
            painter.drawText(QRectF(x, 0,
                                    width, self.tile_rect.height()),
                             text,
                             text_option)
            x += width

    def paint(self, painter: QPainter) -> None:
        self.paint_text_items(painter)
        self.paint_thumbnail(painter)

        if self.hovering and self.animation_timer is None:
            if not self.fileinfo.isdir() and not self.fileinfo.is_archive():
                painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_Overlay)
                painter.setOpacity(0.75)
                self.paint_thumbnail(painter)
                painter.setOpacity(1.0)
                painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)

        if self.level_of_detail > 1:
            self.paint_metadata(painter)
            self.paint_overlay(painter)

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

        target_width = self.tile_rect.width()

        if fm.boundingRect(tmp).width() > target_width:
            while fm.boundingRect(tmp + "…").width() > target_width:
                tmp = tmp[0:-1]

        if tmp != text:
            text = tmp + "…"

        painter.setFont(font)

        k = [0, 1, 1, 2, 3][self.level_of_detail]
        painter.drawText(QPointF(self.tile_rect.width() / 2 - fm.boundingRect(text).width() / 2,
                                 self.tile_rect.height() - 2 + 16 * row - 16 * k + 14),
                         text)

    def paint_thumbnail(self, painter: QPainter) -> None:
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

            if (self.fileinfo.is_archive() or self.fileinfo.isdir()) and not self.hovering:
                self.paint_icon(painter, self.icon)

    def make_text(self) -> Tuple[str, str, str, str]:
        top_left_text = ""
        top_right_text = ""
        bottom_left_text = ""
        bottom_right_text = ""

        if self.fileinfo.has_metadata('duration'):
            hours, minutes, seconds = split_duration(self.fileinfo.get_metadata('duration'))
            top_left_text = "{:d}:{:02d}:{:02d}".format(hours, minutes, seconds)

        if self.fileinfo.has_metadata('pages'):
            top_left_text = "{:d} pages".format(self.fileinfo.get_metadata('pages'))

        if self.fileinfo.has_metadata('file_count'):
            top_left_text = "{:d} files".format(self.fileinfo.get_metadata('file_count'))

        if self.fileinfo.has_metadata('framerate'):
            top_right_text = "{:g}fps".format(self.fileinfo.get_metadata('framerate'))

        if self.fileinfo.has_metadata('width') and self.fileinfo.has_metadata('height'):
            bottom_left_text = "{}x{}".format(self.fileinfo.get_metadata('width'), self.fileinfo.get_metadata('height'))

        return (top_left_text, top_right_text, bottom_left_text, bottom_right_text)

    def paint_metadata(self, painter: QPainter) -> None:
        font = self.style.font
        fm = self.style.fm
        painter.setFont(font)

        if self.new:
            painter.drawPixmap(QRect(2, 2, 24, 24),
                               self.style.shared_pixmaps.new)

        if self.fileinfo.has_metadata('type') and self.fileinfo.get_metadata("type") == "error":
            painter.drawPixmap(QRect(2, 2, 24, 24),
                               self.style.shared_pixmaps.error)

        top_left_text, top_right_text, bottom_left_text, bottom_right = self.make_text()

        if top_left_text:
            w = fm.boundingRect(top_left_text).width()
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(QColor(255, 255, 255, 160))
            painter.drawRect(0, 0, w + 4, 16)
            painter.setPen(QColor(0, 0, 0))
            painter.drawText(2, 12, top_left_text)

        if top_right_text:
            w = fm.boundingRect(top_right_text).width()
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(QColor(255, 255, 255, 160))
            painter.drawRect(self.thumbnail_rect.width() - w - 4, 0, w + 4, 16)
            painter.setPen(QColor(0, 0, 0))
            painter.drawText(self.thumbnail_rect.width() - w - 2, 12, top_right_text)

        if bottom_left_text:
            w = fm.boundingRect(bottom_left_text).width()
            painter.setPen(Qt.PenStyle.NoPen)
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
            self.paint_icon(painter, self.style.shared_icons.image_missing)

    def paint_tiny_icon(self, painter: QPainter, icon: QIcon) -> None:
        painter.setOpacity(0.5)
        icon.paint(painter, QRect(self.tile_rect.width() - 48, 0, 48, 48))

    def paint_icon(self, painter: QPainter, icon: QIcon) -> None:
        if self._item_style == FileItemStyle.ICON:
            rect = make_unscaled_rect(self.thumbnail_rect.width() * 3 // 4, self.thumbnail_rect.width() * 3 // 4,
                                      self.thumbnail_rect.width(), self.thumbnail_rect.height())
        else:
            rect = make_unscaled_rect(self.thumbnail_rect.width(), self.thumbnail_rect.height(),
                                      self.thumbnail_rect.width(), self.thumbnail_rect.height())

        if self.fileinfo.isdir() or self.fileinfo.is_archive():
            scalable = self.style.shared_scalable(self.thumbnail_rect.width(), self.thumbnail_rect.height(),)
            pixmap = scalable.load_icon_icon(icon, outline=True)
            painter.fillRect(self.thumbnail_rect, QBrush(QColor(255, 255, 255, 160)))
            painter.drawPixmap(0, 0, pixmap)
        else:
            icon.paint(painter, rect)


# EOF #
