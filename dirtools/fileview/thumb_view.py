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


from PyQt5.QtCore import Qt, QRectF
from PyQt5.QtGui import QBrush
from PyQt5.QtWidgets import (
    QGraphicsView,
    QGraphicsScene,
)

from dirtools.fileview.file_info import FileInfo
from dirtools.fileview.file_item import ThumbFileItem


class ThumbView(QGraphicsView):

    def __init__(self, controller):
        super().__init__()
        self.setAcceptDrops(True)

        self.controller = controller
        self.files = []
        self.scene = QGraphicsScene()
        self.setScene(self.scene)

        self.tn_width = 256
        self.tn_height = 256
        self.tn_size = min(self.tn_width, self.tn_height)
        self.flavor = "large"

        self.setBackgroundBrush(QBrush(Qt.white, Qt.SolidPattern))

    def dragMoveEvent(self, e):
        # the default implementation will check if any item in the
        # scene accept a drop event, we don't want that, so we
        # override the function to do nothing
        pass

    def dragEnterEvent(self, e):
        print(e.mimeData().formats())
        if e.mimeData().hasFormat("text/uri-list"):
            e.accept()
        else:
            e.ignore()

    def dropEvent(self, e):
        urls = e.mimeData().urls()
        # [PyQt5.QtCore.QUrl('file:///home/ingo/projects/dirtool/trunk/setup.py')]
        self.add_files([url.path() for url in urls])

    def add_files(self, files):
        self.files += files
        self.thumbnails = []

        for filename in self.files:
            thumb = ThumbFileItem(FileInfo(filename), self.controller)
            self.scene.addItem(thumb)
            self.thumbnails.append(thumb)

        self.layout_thumbnails()

    def resizeEvent(self, ev):
        super().resizeEvent(ev)
        self.layout_thumbnails()

    def layout_thumbnails(self):
        xs = self.tn_width + 8
        ys = self.tn_height + 8 + 16
        x = 0
        y = 0
        for thumb in self.thumbnails:
            thumb.setPos(x, y)
            x += xs
            if x + xs > self.viewport().width():
                y += ys
                x = 0

        if True:  # top/left alignment
            bounding_rect = QRectF(0,
                                   0,
                                   max(self.viewport().size().width(),
                                       self.scene.itemsBoundingRect().width()),
                                   max(self.viewport().size().height(),
                                       self.scene.itemsBoundingRect().height()))
            self.setSceneRect(bounding_rect)
        else:
            self.setSceneRect(self.scene.itemsBoundingRect())

    def zoom_in(self):
        self.tn_width = 256
        self.tn_height = 256
        self.tn_size = min(self.tn_width, self.tn_height)
        self.flavor = "large"

        for thumb in self.thumbnails:
            thumb.refresh()
        self.layout_thumbnails()

    def zoom_out(self):
        self.tn_width = 128
        self.tn_height = 128
        self.tn_size = min(self.tn_width, self.tn_height)
        self.flavor = "normal"

        for thumb in self.thumbnails:
            thumb.refresh()
        self.layout_thumbnails()


# EOF #
