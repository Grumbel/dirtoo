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
from PyQt5.QtGui import QPixmap, QColor, QPen, QFontMetrics
from PyQt5.QtWidgets import (
    QGraphicsTextItem,
    QGraphicsPixmapItem,
    QGraphicsRectItem,
)

import bytefmt

from dirtools.fileview.file_item import FileItem
from dirtools.fileview.thumbnail_cache import ThumbnailStatus


class ThumbFileItem(FileItem):

    def __init__(self, fileinfo, controller, thumb_view):
        logging.debug("ThumbFileItem.__init__: %s", fileinfo.abspath())
        self.thumb_view = thumb_view
        self.pixmap_item = None
        self.lock_item = None
        self.thumbnail_status = ThumbnailStatus.NONE
        super().__init__(fileinfo, controller)

    def paint(self, *args):
        if self.pixmap_item is None:
            self.make_thumbnail()

        super().paint(*args)

    def hoverEnterEvent(self, ev):
        self.setZValue(2.0)
        # self.text.setVisible(True)
        # self.text.setDefaultTextColor(QColor(255, 255, 255))
        self.select_rect.setVisible(True)
        self.controller.show_current_filename(self.fileinfo.filename())

    def hoverLeaveEvent(self, ev):
        self.setZValue(0)
        self.select_rect.setVisible(False)
        # self.text.setVisible(False)
        # self.text.setDefaultTextColor(QColor(0, 0, 0))
        self.controller.show_current_filename("")

    def add_text_item(self, row, text):
        text_item = QGraphicsTextItem(text)
        font = text_item.font()
        font.setPixelSize(10)
        text_item.setFont(font)
        text_item.setDefaultTextColor(QColor(0, 0, 0))
        text_item.setAcceptHoverEvents(False)

        fm = QFontMetrics(font)
        tmp = text
        if fm.width(tmp) > self.boundingRect().width():
            while fm.width(tmp + "…") > self.boundingRect().width():
                tmp = tmp[0:-1]

        if tmp == text:
            text_item.setPlainText(tmp)
        else:
            text_item.setPlainText(tmp + "…")

        text_item.setPos(self.pos().x() + self.thumb_view.tn_width / 2 - text_item.boundingRect().width() / 2,
                         self.pos().y() + self.thumb_view.tn_height - 2 + 16 * row)

        # text_item.setVisible(False)
        self.addToGroup(text_item)

    def make_items(self):
        rect = QGraphicsRectItem(self.pos().x() - 2,
                                 self.pos().y() - 2,
                                 self.thumb_view.tn_width + 4,
                                 self.thumb_view.tn_height + 4 + 16 * self.thumb_view.level_of_detail)
        rect.setPen(QPen(Qt.NoPen))
        rect.setBrush(QColor(192 + 32, 192 + 32, 192 + 32))
        self.addToGroup(rect)

        self.select_rect = QGraphicsRectItem(self.pos().x() - 2,
                                             self.pos().y() - 2,
                                             self.thumb_view.tn_width + 4,
                                             self.thumb_view.tn_height + 4 + 16 * self.thumb_view.level_of_detail)
        self.select_rect.setPen(QPen(Qt.NoPen))
        self.select_rect.setBrush(QColor(192, 192, 192))
        self.select_rect.setVisible(False)
        self.select_rect.setAcceptHoverEvents(False)
        self.addToGroup(self.select_rect)

        if self.thumb_view.level_of_detail > 0:
            self.add_text_item(0, self.fileinfo.basename())

        if self.thumb_view.level_of_detail > 1:
            self.add_text_item(1, self.fileinfo.ext())

        if self.thumb_view.level_of_detail > 2:
            self.add_text_item(2, bytefmt.humanize(self.fileinfo.size()))

        if self.thumb_view.level_of_detail > 3:
            dt = datetime.fromtimestamp(self.fileinfo.mtime())
            self.add_text_item(3, dt.strftime("%F %T"))

        # tooltips don't work for the whole group
        # tooltips break the hover events!
        # self.text.setToolTip(self.fileinfo.filename())
        # self.make_thumbnail()

    def make_pixmap(self):
        pixmap = self.thumb_view.pixmap_from_fileinfo(self.fileinfo, 3 * self.thumb_view.tn_size // 4)
        if pixmap is not None:
            return pixmap
        else:
            result = self.controller.request_thumbnail(self.fileinfo,
                                                       flavor=self.thumb_view.flavor)
            filename, status = result
            self.thumbnail_status = status

            if status == ThumbnailStatus.OK:
                logging.debug("%s: ThumbFileItem.make_pixmap: loading thumbnail", self)
                pixmap = QPixmap(filename)
                if pixmap.isNull():
                    logging.error("ThumbFileItem.make_pixmap: failed to load %s based on %s",
                                  filename, self.fileinfo.abspath())
                    return self.thumb_view.shared_pixmaps.image_missing
                else:
                    # Scale it to fit
                    w = pixmap.width() * self.thumb_view.tn_width // pixmap.height()
                    h = pixmap.height() * self.thumb_view.tn_height // pixmap.width()
                    if w <= self.thumb_view.tn_width:
                        pixmap = pixmap.scaledToHeight(self.thumb_view.tn_height,
                                                       Qt.SmoothTransformation)
                    elif h <= self.thumb_view.tn_height:
                        pixmap = pixmap.scaledToWidth(self.thumb_view.tn_width,
                                                      Qt.SmoothTransformation)
                    return pixmap

            elif status == ThumbnailStatus.LOADING:
                return self.thumb_view.shared_pixmaps.image_loading

            else:  # if status == ThumbnailStatus.ERROR
                return self.thumb_view.shared_pixmaps.image_missing

    def make_thumbnail(self):
        pixmap = self.make_pixmap()

        self.pixmap_item = QGraphicsPixmapItem(pixmap)

        self.pixmap_item.setPos(
            self.pos().x() + self.thumb_view.tn_width / 2 - pixmap.width() / 2,
            self.pos().y() + self.thumb_view.tn_height / 2 - pixmap.height() / 2)
        self.addToGroup(self.pixmap_item)

        if not self.fileinfo.have_access():
            pixmap = self.thumb_view.shared_pixmaps.locked
            self.lock_item = QGraphicsPixmapItem(pixmap)
            self.lock_item.setOpacity(0.5)
            self.lock_item.setPos(
                self.pos().x() + self.thumb_view.tn_width / 2 - pixmap.width() / 2,
                self.pos().y() + self.thumb_view.tn_height / 2 - pixmap.height() / 2)
            self.addToGroup(self.lock_item)

    def boundingRect(self):
        return QRectF(0, 0, self.thumb_view.tn_width, self.thumb_view.tn_height)

    def reload_pixmap(self):
        if self.pixmap_item is not None:
            pixmap = self.make_pixmap()
            self.pixmap_item.setPixmap(pixmap)
            self.pixmap_item.setPos(
                self.thumb_view.tn_width / 2 - pixmap.width() / 2,
                self.thumb_view.tn_height / 2 - pixmap.height() / 2)

    def reload(self, x=0, y=0):
        for item in self.childItems():
            self.removeFromGroup(item)
        self.setPos(x, y)
        self.pixmap_item = None
        self.lock_item = None
        self.make_items()

    def reload_thumbnail(self):
        self.reload(x=self.pos().x(), y=self.pos().y())


# EOF #
