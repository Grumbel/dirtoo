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


from typing import List

from PyQt5.QtCore import Qt, QRectF
from PyQt5.QtGui import QBrush
from PyQt5.QtWidgets import (
    QGraphicsView,
    QGraphicsScene,
)

from dirtools.fileview.thumb_file_item import ThumbFileItem


class ThumbView(QGraphicsView):

    def __init__(self, controller):
        super().__init__()
        self.setAcceptDrops(True)

        self.controller = controller
        self.scene = QGraphicsScene()
        self.setScene(self.scene)

        self.padding = 16
        self.thumbnails: List[ThumbFileItem] = []

        self.zoom_index = 2
        self.apply_zoom()

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
        self.controller.add_files([url.path() for url in urls])

    def set_file_collection(self, file_collection):
        self.file_collection = file_collection
        self.file_collection.sig_files_set.connect(self.on_file_collection_set)
        self.file_collection.sig_files_reordered.connect(self.on_file_collection_set)
        self.on_file_collection_set()

    def on_file_collection_set(self):
        fileinfos = self.file_collection.get_fileinfos()

        self.scene.clear()
        self.thumbnails = []

        for fileinfo in fileinfos:
            if not fileinfo.is_filtered:
                thumb = ThumbFileItem(fileinfo, self.controller)
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
            h = max(self.viewport().size().height(), bottom_y)

            bounding_rect = QRectF(0, 0, w, h)
            self.setSceneRect(bounding_rect)
        else:  # center alignment
            self.setSceneRect(self.scene.itemsBoundingRect())

    def zoom_in(self):
        self.zoom_index += 1
        if self.zoom_index > 4:
            self.zoom_index = 4
        self.apply_zoom()

    def zoom_out(self):
        self.zoom_index -= 1
        if self.zoom_index < 0:
            self.zoom_index = 0
        self.apply_zoom()

    def apply_zoom(self):
        # zoom_levels = ["Tiny", "Small" "Normal", "Large", "Extra Large"]
        self.tn_width = 32 * 2 ** self.zoom_index
        self.tn_height = 32 * 2 ** self.zoom_index
        self.tn_size = min(self.tn_width, self.tn_height)

        if self.zoom_index < 3:
            self.flavor = "normal"
        else:
            self.flavor = "large"

        self.reload()

    def reload(self):
        for thumb in self.thumbnails:
            thumb.reload()
        self.layout_thumbnails()


# EOF #
