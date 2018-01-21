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
from datetime import datetime

from PyQt5.QtCore import Qt, QRectF
from PyQt5.QtGui import QPixmap, QColor, QPen, QFontMetrics, QFont

import bytefmt

from dirtools.fileview.file_item import FileItem
from dirtools.fileview.thumbnail_cache import ThumbnailStatus


class ThumbFileItem(FileItem):

    def __init__(self, fileinfo, controller, thumb_view):
        super().__init__(fileinfo, controller)

        logging.debug("ThumbFileItem.__init__: %s", fileinfo.abspath())
        self.thumb_view = thumb_view
        self.pixmap = None
        self.thumbnail_status = ThumbnailStatus.NONE
        self.hovering = False
        # self.make_items()

    def paint(self, painter, option, widget):
        logging.debug("ThumbFileItem.paint_items: %s", self.fileinfo.abspath())

        # background rectangle
        painter.fillRect(-2, -2, self.thumb_view.tn_width + 4,
                         self.thumb_view.tn_height + 4 + 16 * self.thumb_view.level_of_detail,
                         QColor(192 + 32, 192 + 32, 192 + 32))

        # hover rectangle
        if self.hovering:
            painter.fillRect(-2,
                             -2,
                             self.thumb_view.tn_width + 4,
                             self.thumb_view.tn_height + 4 + 16 * self.thumb_view.level_of_detail,
                             QColor(192, 192, 192))

        self.paint_text_items(painter)

        pixmap = self.pixmap or self.make_pixmap()
        self.paint_thumbnail(painter, pixmap)

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
        if fm.width(tmp) > self.boundingRect().width():
            while fm.width(tmp + "…") > self.boundingRect().width():
                tmp = tmp[0:-1]

        if tmp != text:
            text = tmp + "…"

        painter.setFont(font)
        painter.drawText(self.thumb_view.tn_width / 2 - fm.width(text) / 2,
                         self.thumb_view.tn_height - 2 + 16 * row + 14,
                         text)

    def paint_thumbnail(self, painter, pixmap):
        logging.debug("ThumbFileItem.make_thumbnail: %s", self.fileinfo.abspath())

        if pixmap is not None:
            painter.drawPixmap(
                self.thumb_view.tn_width / 2 - pixmap.width() / 2,
                self.thumb_view.tn_height / 2 - pixmap.height() / 2,
                pixmap)

        if not self.fileinfo.have_access():
            painter.setOpacity(0.5)
            painter.drawPixmap(
                self.thumb_view.tn_width / 2 - pixmap.width() / 2,
                self.thumb_view.tn_height / 2 - pixmap.height() / 2,
                self.thumb_view.shared_pixmaps.locked)

    def make_pixmap(self):
        logging.debug("ThumbFileItem.make_pixmap: %s", self.fileinfo.abspath())

        # try to load shared pixmaps, this is fast
        pixmap = self.thumb_view.pixmap_from_fileinfo(self.fileinfo, 3 * self.thumb_view.tn_size // 4)
        if pixmap is not None:
            return pixmap
        else:
            # no shared pixmap found, so try to generate a thumbnail
            result = self.controller.request_thumbnail(self.fileinfo,
                                                       flavor=self.thumb_view.flavor)
            filename, status = result
            self.thumbnail_status = status

            # a thumbnail does already exist, just load it
            if status == ThumbnailStatus.OK:
                logging.debug("%s: ThumbFileItem.make_pixmap: loading thumbnail", self)
                self.thumb_view.thumbnail_request(self, filename)

            elif status == ThumbnailStatus.LOADING:
                return self.thumb_view.shared_pixmaps.image_loading

            else:  # if status == ThumbnailStatus.ERROR
                return self.thumb_view.shared_pixmaps.image_missing

    def hoverEnterEvent(self, ev):
        self.hovering = True
        self.controller.show_current_filename(self.fileinfo.filename())
        self.update()

    def hoverLeaveEvent(self, ev):
        self.hovering = False
        self.controller.show_current_filename("")
        self.update()

    def set_pixmap(self, pixmap):
        self.pixmap = pixmap
        if pixmap is not None:
            self.update()

    def boundingRect(self):
        return QRectF(0, 0,
                      self.thumb_view.tn_width,
                      self.thumb_view.tn_height + 16 * self.thumb_view.level_of_detail)

    def reload_pixmap(self, filename):
        self.thumb_view.thumbnail_request(self, filename)

    def reload(self, x=0, y=0):
        # self.setPos(x, y)
        self.pixmap = None
        self.update()

    def reload_thumbnail(self):
        self.reload(x=self.pos().x(), y=self.pos().y())


# EOF #
