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


import html
import os
import subprocess

from PyQt5.QtCore import Qt, QRectF, QMimeData, QUrl
from PyQt5.QtGui import QPixmap, QColor, QPen, QIcon, QDrag
from PyQt5.QtWidgets import (
    QGraphicsItem,
    QGraphicsItemGroup,
    QGraphicsTextItem,
    QGraphicsPixmapItem,
    QGraphicsRectItem,
)

from dirtools.thumbnail import make_thumbnail_filename
from dirtools.util import expand_file


windows = []


class FileItem(QGraphicsItemGroup):

    def __init__(self, filename, controller):
        super().__init__()
        self.thumbnail = None
        self.filename = filename
        self.abspath = os.path.abspath(filename)
        self.dirname = os.path.dirname(self.abspath)
        self.basename = os.path.basename(self.abspath)
        self.controller = controller
        self.setAcceptHoverEvents(True)

        # allow moving objects around
        # self.setFlag(QGraphicsItem.ItemIsMovable, True)

        self.text = None
        self.make_items()
        self.show_abspath()

        self.press_pos = None
        self.dragging = False

    def mousePressEvent(self, ev):
        # PyQt will route the event to the child items when we don't
        # override this
        self.press_pos = ev.pos()


    def mouseMoveEvent(self, ev):
        if not self.dragging and (ev.pos() - self.press_pos).manhattanLength() > 16:
            print("drag start")
            self.dragging = True

            mimeData = QMimeData()
            mimeData.setUrls([QUrl("file://" + self.filename)])
            self.drag = QDrag(self.controller)
            self.drag.setPixmap(self.pixmap)
            self.drag.setMimeData(mimeData)
            # drag.setHotSpot(e.pos() - self.rect().topLeft())
            self.dropAction = self.drag.exec_(Qt.CopyAction)

    def mouseReleaseEvent(self, ev):
        if self.dragging:
            pass
        else:
            if os.path.isdir(self.filename):
                from dirtools.fileview.file_view_window import FileViewWindow
                window = FileViewWindow()
                files = expand_file(self.filename, recursive=False)
                window.file_view.add_files(files)
                window.thumb_view.add_files(files)
                window.show()
                windows.append(window)
            else:
                subprocess.Popen(["xdg-open", self.filename])

        self.dragging = False

    def show_basename(self):
        pass

    def show_abspath(self):
        pass


class DetailFileItem(FileItem):

    def hoverEnterEvent(self, ev):
        self.show_thumbnail()
        self.text.setDefaultTextColor(QColor(0, 128, 128))
        self.controller.set_filename(self.filename)

    def hoverLeaveEvent(self, ev):
        self.hide_thumbnail()
        self.text.setDefaultTextColor(QColor(0, 0, 0))
        self.controller.set_filename("")

    def make_items(self):
        self.text = QGraphicsTextItem()
        # tooltips don't work for the whole group
        self.text.setToolTip(self.filename)
        self.addToGroup(self.text)

    def show_thumbnail(self):
        if self.thumbnail is None:
            thumbnail_filename = make_thumbnail_filename(self.filename)
            if thumbnail_filename is None:
                print("no thumbnail for", self.filename)
            else:
                print("showing thumbnail:", thumbnail_filename)
                self.thumbnail = QGraphicsPixmapItem(QPixmap(thumbnail_filename))
                self.thumbnail.setPos(self.pos())
                self.addToGroup(self.thumbnail)
                self.thumbnail.show()

    def hide_thumbnail(self):
        if self.thumbnail is not None:
            # print("hiding thumbnail", self.filename)
            self.removeFromGroup(self.thumbnail)
            self.thumbnail = None

    def show_abspath(self):
        if len(self.basename) < 80:
            html_text = '<font color="grey">{}/</font>{}'.format(
                html.escape(self.dirname),
                html.escape(self.basename))
        else:
            html_text = '<font color="grey">{}/</font>{}<font color="grey">…</font>'.format(
                html.escape(self.dirname),
                html.escape(self.basename[0:80]))

        self.text.setHtml(html_text)

    def show_basename(self):
        self.text.setPlainText(self.basename)


class ThumbFileItem(FileItem):

    def hoverEnterEvent(self, ev):
        self.setZValue(2.0)
        # self.text.setVisible(True)
        self.text.setDefaultTextColor(QColor(255, 255, 255))
        self.rect.setVisible(True)
        self.controller.set_filename(self.filename)

    def hoverLeaveEvent(self, ev):
        self.setZValue(0)
        self.rect.setVisible(False)
        # self.text.setVisible(False)
        self.text.setDefaultTextColor(QColor(0, 0, 0))
        self.controller.set_filename("")

    def make_items(self):
        rect = QGraphicsRectItem(-2, -2, 128 + 4, 128 + 4 + 16)
        rect.setPen(QPen(Qt.NoPen))
        rect.setBrush(QColor(192+32, 192+32, 192+32))
        self.addToGroup(rect)

        self.rect = QGraphicsRectItem(-8, -8, 128 + 16, 128 + 24 + 8)
        self.rect.setBrush(QColor(32, 32, 32))
        self.rect.setVisible(False)
        self.rect.setAcceptHoverEvents(False)
        self.addToGroup(self.rect)

        text = self.basename
        if len(text) > 20:
            text = text[0:20] + "…"
        self.text = QGraphicsTextItem(text)
        font = self.text.font()
        font.setPixelSize(10)
        self.text.setFont(font)
        self.text.setDefaultTextColor(QColor(0, 0, 0))
        self.text.setAcceptHoverEvents(False)
        self.text.setPos(64 - self.text.boundingRect().width()/2, 128)
        # self.text.setVisible(False)
        self.addToGroup(self.text)

        # tooltips don't work for the whole group
        # tooltips break the however events!
        # self.text.setToolTip(self.filename)

        thumbnail_filename = make_thumbnail_filename(self.filename)
        if os.path.isdir(self.filename):
            pixmap = QIcon.fromTheme("folder").pixmap(96)
        elif thumbnail_filename:
            pixmap = QPixmap(thumbnail_filename)
        else:
            pixmap = QIcon.fromTheme("error").pixmap(96)
        self.pixmap = pixmap

        thumb = QGraphicsPixmapItem(pixmap)
        thumb.setPos(64 - pixmap.width() / 2,
                     64 - pixmap.height() / 2)
        self.addToGroup(thumb)

    def boundingRect(self):
        return QRectF(0, 0, 128, 128)


# EOF #
