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


import os

from PyQt5.QtCore import Qt, QRectF
from PyQt5.QtGui import QPixmap, QColor, QPen, QIcon, QFontMetrics
from PyQt5.QtWidgets import (
    QGraphicsTextItem,
    QGraphicsPixmapItem,
    QGraphicsRectItem,
)

from dirtools.thumbnail import make_thumbnail_filename
from dirtools.fileview.file_item import FileItem


def pixmap_from_filename(filename, tn_size):
    tn_size = 3 * tn_size // 4

    _, ext = os.path.splitext(filename)
    if ext == ".rar":
        return QIcon.fromTheme("rar").pixmap(tn_size)
    elif ext == ".zip":
        return QIcon.fromTheme("zip").pixmap(tn_size)
    elif ext == ".txt":
        return QIcon.fromTheme("txt").pixmap(tn_size)
    else:
        return QPixmap()


class ThumbFileItem(FileItem):

    def __init__(self, *args):
        self.pixmap_item = None
        self.lock_item = None
        super().__init__(*args)

    def paint(self, *args):
        if self.pixmap_item is None:
            self.make_thumbnail()
        super().paint(*args)

    def hoverEnterEvent(self, ev):
        self.setZValue(2.0)
        # self.text.setVisible(True)
        # self.text.setDefaultTextColor(QColor(255, 255, 255))
        self.select_rect.setVisible(True)
        self.controller.show_current_filename(self.fileinfo.filename)

    def hoverLeaveEvent(self, ev):
        self.setZValue(0)
        self.select_rect.setVisible(False)
        # self.text.setVisible(False)
        self.text.setDefaultTextColor(QColor(0, 0, 0))
        self.controller.show_current_filename("")

    def make_items(self):
        rect = QGraphicsRectItem(-2, -2, self.controller.tn_width + 4, self.controller.tn_height + 4 + 16)
        rect.setPen(QPen(Qt.NoPen))
        rect.setBrush(QColor(192 + 32, 192 + 32, 192 + 32))
        self.addToGroup(rect)

        self.select_rect = QGraphicsRectItem(-2, -2, self.controller.tn_width + 4, self.controller.tn_height + 4 + 16)
        self.select_rect.setPen(QPen(Qt.NoPen))
        self.select_rect.setBrush(QColor(192, 192, 192))
        self.select_rect.setVisible(False)
        self.select_rect.setAcceptHoverEvents(False)
        self.addToGroup(self.select_rect)

        text = self.fileinfo.basename
        self.text = QGraphicsTextItem(text)
        font = self.text.font()
        font.setPixelSize(10)
        self.text.setFont(font)
        self.text.setDefaultTextColor(QColor(0, 0, 0))
        self.text.setAcceptHoverEvents(False)

        fm = QFontMetrics(font)
        tmp = text
        if fm.width(tmp) > self.boundingRect().width():
            while fm.width(tmp + "…") > self.boundingRect().width():
                tmp = tmp[0:-1]

        if tmp == text:
            self.text.setPlainText(tmp)
        else:
            self.text.setPlainText(tmp + "…")

        self.text.setPos(self.controller.tn_width / 2 - self.text.boundingRect().width() / 2,
                         self.controller.tn_height - 2)

        # self.text.setVisible(False)
        self.addToGroup(self.text)

        # tooltips don't work for the whole group
        # tooltips break the hover events!
        # self.text.setToolTip(self.fileinfo.filename)
        # self.make_thumbnail()

    def make_thumbnail(self):
        thumbnail_filename = make_thumbnail_filename(self.fileinfo.filename, flavor=self.controller.flavor)

        if self.fileinfo.isdir:
            pixmap = QIcon.fromTheme("folder").pixmap(3 * self.controller.tn_size // 4)
        elif thumbnail_filename:
            pixmap = QPixmap(thumbnail_filename)

            w = pixmap.width() * self.controller.tn_width // pixmap.height()
            h = pixmap.height() * self.controller.tn_height // pixmap.width()
            if w <= self.controller.tn_width:
                pixmap = pixmap.scaledToHeight(self.controller.tn_height,
                                               Qt.SmoothTransformation)
            elif h <= self.controller.tn_height:
                pixmap = pixmap.scaledToWidth(self.controller.tn_width,
                                              Qt.SmoothTransformation)
        else:
            pixmap = pixmap_from_filename(self.fileinfo.filename, 3 * self.controller.tn_size // 4)
            if pixmap.isNull():
                pixmap = QIcon.fromTheme("error").pixmap(3 * self.controller.tn_size // 4)

        self.pixmap_item = QGraphicsPixmapItem(pixmap)
        self.pixmap_item.setPos(
            self.pos().x() + self.controller.tn_width / 2 - pixmap.width() / 2,
            self.pos().y() + self.controller.tn_height / 2 - pixmap.height() / 2)
        self.addToGroup(self.pixmap_item)

        if not self.fileinfo.have_access:
            pixmap = QIcon.fromTheme("locked").pixmap(3 * self.controller.tn_size // 4)
            self.lock_item = QGraphicsPixmapItem(pixmap)
            self.lock_item.setOpacity(0.5)
            self.lock_item.setPos(
                self.pos().x() + self.controller.tn_width / 2 - pixmap.width() / 2,
                self.pos().y() + self.controller.tn_height / 2 - pixmap.height() / 2)
            self.addToGroup(self.lock_item)

    def boundingRect(self):
        return QRectF(0, 0, self.controller.tn_width, self.controller.tn_height)

    def reload(self):
        for item in self.childItems():
            self.removeFromGroup(item)
        self.setPos(0, 0)
        self.pixmap_item = None
        self.lock_item = None
        self.make_items()


# EOF #
