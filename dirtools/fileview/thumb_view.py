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

        self.padding = 16

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

    def set_files(self, files):
        self.scene.clear()
        self.files = files
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
        tile_w = self.tn_width
        tile_h = self.tn_height + 16

        x_spacing = 16
        y_spacing = 16

        x_step = tile_w + x_spacing
        y_step = tile_h + y_spacing

        threshold = self.viewport().width() - self.padding

        x = self.padding
        y = self.padding

        right_x = 0
        bottom_y = 0

        for thumb in self.thumbnails:
            right_x = max(x, right_x)
            bottom_y = y
            thumb.setPos(x, y)
            x += x_step
            if x + tile_w >= threshold:
                y += y_step
                x = self.padding

        if True:  # top/left alignment
            right_x += tile_w + self.padding
            bottom_y += tile_h + self.padding

            if True:  # center alignment
                w = right_x
            else:
                w = max(self.viewport().size().width(), right_x)
            h = max(self.viewport().size().height(),bottom_y)

            bounding_rect = QRectF(0, 0, w, h)
            self.setSceneRect(bounding_rect)
        else:  # center alignment
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
