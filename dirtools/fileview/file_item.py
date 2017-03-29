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

from PyQt5.QtGui import QPixmap, QColor
from PyQt5.QtWidgets import (QGraphicsItemGroup, QGraphicsTextItem, QGraphicsPixmapItem)

from dirtools.thumbnail import make_thumbnail_filename


class FileItem(QGraphicsItemGroup):

    def __init__(self, filename):
        super().__init__()
        self.thumbnail = None
        self.filename = filename
        self.abspath = os.path.abspath(filename)
        self.dirname = os.path.dirname(filename)
        self.basename = os.path.basename(filename)

        self.text = QGraphicsTextItem()
        self.text.setToolTip(self.filename)
        self.addToGroup(self.text)

        self.show_abspath()

    def mousePressEvent(self, ev):
        subprocess.Popen(["xdg-open", self.filename])

    def hoverEnterEvent(self, ev):
        self.show_thumbnail()
        self.text.setDefaultTextColor(QColor(0, 128, 128))

    def hoverLeaveEvent(self, ev):
        self.hide_thumbnail()
        self.text.setDefaultTextColor(QColor(0, 0, 0))

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


# EOF #
