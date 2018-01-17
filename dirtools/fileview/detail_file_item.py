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
import datetime

from PyQt5.QtGui import QPixmap, QColor
from PyQt5.QtWidgets import (
    QGraphicsTextItem,
    QGraphicsPixmapItem,
)

import bytefmt

from dirtools.fileview.file_item import FileItem


class DetailFileItem(FileItem):

    def __init__(self, *args):
        self.date_item = None
        self.filename_item = None
        super().__init__(*args)

    def make_items(self):
        date = datetime.datetime.fromtimestamp(self.fileinfo.stat().st_mtime)
        self.date_item = QGraphicsTextItem(date.strftime("%F %T"))
        self.date_item.setPos(0, 0)
        self.addToGroup(self.date_item)

        if self.fileinfo.isdir():
            self.size_item = QGraphicsTextItem("[DIR]")
        else:
            self.size_item = QGraphicsTextItem(bytefmt.humanize(self.fileinfo.stat().st_size))
        self.size_item.setPos(225 - self.size_item.boundingRect().width(), 0)
        self.addToGroup(self.size_item)

        self.filename_item = QGraphicsTextItem(self.fileinfo.filename())
        self.filename_item.setPos(250, 0)
        self.addToGroup(self.filename_item)

        # tooltips don't work for the whole group
        self.filename_item.setToolTip(self.fileinfo.filename())

    def hoverEnterEvent(self, ev):
        self.show_thumbnail()
        self.filename_item.setDefaultTextColor(QColor(0, 128, 128))
        self.controller.show_current_filename(self.fileinfo.filename())

    def hoverLeaveEvent(self, ev):
        self.hide_thumbnail()
        self.filename_item.setDefaultTextColor(QColor(0, 0, 0))
        self.controller.show_current_filename("")

    def show_thumbnail(self):
        if self.thumbnail is None:
            thumbnail_filename = self.controller.request_thumbnail(self.fileinfo, flavor="normal")
            if thumbnail_filename is None:
                print("no thumbnail for", self.fileinfo.filename())
            else:
                print("showing thumbnail:", thumbnail_filename)
                self.thumbnail = QGraphicsPixmapItem(QPixmap(thumbnail_filename))
                self.thumbnail.setPos(self.pos())
                self.addToGroup(self.thumbnail)
                self.thumbnail.show()

    def hide_thumbnail(self):
        if self.thumbnail is not None:
            # print("hiding thumbnail", self.fileinfo.filename())
            self.removeFromGroup(self.thumbnail)
            self.thumbnail = None

    def show_abspath(self):
        if len(self.fileinfo.basename()) < 80:
            html_text = '<font color="grey">{}/</font>{}'.format(
                html.escape(self.fileinfo.dirname()),
                html.escape(self.fileinfo.basename()))
        else:
            html_text = '<font color="grey">{}/</font>{}<font color="grey">…</font>'.format(
                html.escape(self.fileinfo.dirname()),
                html.escape(self.fileinfo.basename()[0:80]))

        self.filename_item.setHtml(html_text)

    def show_basename(self):
        self.filename_item.setPlainText(self.fileinfo.basename())


# EOF #
