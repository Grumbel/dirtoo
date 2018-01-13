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
from PyQt5.QtGui import QPixmap, QColor, QPen, QIcon, QDrag, QFontMetrics
from PyQt5.QtWidgets import (
    QGraphicsItemGroup,
    QGraphicsTextItem,
    QGraphicsPixmapItem,
    QGraphicsRectItem,
)

from dirtools.thumbnail import make_thumbnail_filename
from dirtools.util import expand_file


windows = []


class FileItem(QGraphicsItemGroup):

    def __init__(self, fileinfo, controller):
        super().__init__()
        self.fileinfo = fileinfo
        self.thumbnail = None
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
        if ev.button() == Qt.LeftButton:
            self.press_pos = ev.pos()
        elif ev.button() == Qt.RightButton:
            pass

    def mouseMoveEvent(self, ev):
        if not self.dragging and (ev.pos() - self.press_pos).manhattanLength() > 16:
            print("drag start")
            self.dragging = True

            mime_data = QMimeData()
            mime_data.setUrls([QUrl("file://" + self.fileinfo.abspath)])
            self.drag = QDrag(self.controller)
            self.drag.setPixmap(self.pixmap)
            self.drag.setMimeData(mime_data)
            # drag.setHotSpot(e.pos() - self.select_rect().topLeft())
            self.dropAction = self.drag.exec_(Qt.CopyAction)

    def mouseReleaseEvent(self, ev):
        if ev.button() == Qt.LeftButton:
            if self.dragging:
                pass
            else:
                if self.fileinfo.isdir:
                    from dirtools.fileview.file_view_window import FileViewWindow
                    window = FileViewWindow()
                    files = expand_file(self.fileinfo.filename, recursive=False)
                    window.file_view.set_files(files)
                    window.thumb_view.set_files(files)
                    window.show()
                    windows.append(window)
                else:
                    subprocess.Popen(["xdg-open", self.fileinfo.filename])

            self.dragging = False

    def show_basename(self):
        pass

    def show_abspath(self):
        pass


class DetailFileItem(FileItem):

    def __init__(self, *args):
        super().__init__(*args)

    def hoverEnterEvent(self, ev):
        self.show_thumbnail()
        self.text.setDefaultTextColor(QColor(0, 128, 128))
        self.controller.show_current_filename(self.fileinfo.filename)

    def hoverLeaveEvent(self, ev):
        self.hide_thumbnail()
        self.text.setDefaultTextColor(QColor(0, 0, 0))
        self.controller.show_current_filename("")

    def make_items(self):
        self.text = QGraphicsTextItem()
        # tooltips don't work for the whole group
        self.text.setToolTip(self.fileinfo.filename)
        self.addToGroup(self.text)

    def show_thumbnail(self):
        if self.thumbnail is None:
            thumbnail_filename = make_thumbnail_filename(self.fileinfo.filename)
            if thumbnail_filename is None:
                print("no thumbnail for", self.fileinfo.filename)
            else:
                print("showing thumbnail:", thumbnail_filename)
                self.thumbnail = QGraphicsPixmapItem(QPixmap(thumbnail_filename))
                self.thumbnail.setPos(self.pos())
                self.addToGroup(self.thumbnail)
                self.thumbnail.show()

    def hide_thumbnail(self):
        if self.thumbnail is not None:
            # print("hiding thumbnail", self.fileinfo.filename)
            self.removeFromGroup(self.thumbnail)
            self.thumbnail = None

    def show_abspath(self):
        if len(self.fileinfo.basename) < 80:
            html_text = '<font color="grey">{}/</font>{}'.format(
                html.escape(self.fileinfo.dirname),
                html.escape(self.fileinfo.basename))
        else:
            html_text = '<font color="grey">{}/</font>{}<font color="grey">…</font>'.format(
                html.escape(self.fileinfo.dirname),
                html.escape(self.fileinfo.basename[0:80]))

        self.text.setHtml(html_text)

    def show_basename(self):
        self.text.setPlainText(self.fileinfo.basename)


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
        super().__init__(*args)

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

        fm = QFontMetrics(font);
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

        thumbnail_filename = make_thumbnail_filename(self.fileinfo.filename, flavor=self.controller.flavor)
        if self.fileinfo.isdir:
            pixmap = QIcon.fromTheme("folder").pixmap(3 * self.controller.tn_size // 4)
        elif thumbnail_filename:
            pixmap = QPixmap(thumbnail_filename)
        else:
            pixmap = pixmap_from_filename(self.fileinfo.filename, 3 * self.controller.tn_size // 4)
            if pixmap.isNull():
                pixmap = QIcon.fromTheme("error").pixmap(3 * self.controller.tn_size // 4)
        self.pixmap = pixmap

        self.thumbo = QGraphicsPixmapItem(pixmap)
        self.thumbo.setPos(self.controller.tn_width / 2 - pixmap.width() / 2,
                           self.controller.tn_height / 2 - pixmap.height() / 2)
        self.addToGroup(self.thumbo)

    def boundingRect(self):
        return QRectF(0, 0, self.controller.tn_width, self.controller.tn_height)

    def refresh(self):
        for item in self.childItems():
            self.scene().removeItem(item)
        self.setPos(0, 0)
        self.make_items()


# EOF #
