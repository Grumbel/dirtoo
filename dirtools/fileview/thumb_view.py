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

from typing import List, Dict

from PyQt5.QtCore import Qt, QRectF
from PyQt5.QtGui import QBrush, QIcon
from PyQt5.QtWidgets import (
    QGraphicsView,
    QGraphicsScene,
)

from dirtools.fileview.thumb_file_item import ThumbFileItem


class SharedPixmaps:

    def __init__(self, tn_size):
        tn_size = tn_size // 4 * 3
        self.folder = QIcon.fromTheme("folder").pixmap(tn_size)
        self.rar = QIcon.fromTheme("rar").pixmap(tn_size)
        self.zip = QIcon.fromTheme("zip").pixmap(tn_size)
        self.txt = QIcon.fromTheme("txt").pixmap(tn_size)
        self.image_loading = QIcon.fromTheme("image-loading").pixmap(tn_size)
        self.image_missing = QIcon.fromTheme("image-missing").pixmap(tn_size)
        self.locked = QIcon.fromTheme("locked").pixmap(tn_size)


class ThumbView(QGraphicsView):

    def __init__(self, controller):
        super().__init__()

        self.show_filtered = False

        self.abspath2item: Dict[str, ThumbFileItem] = {}
        self.setAcceptDrops(True)

        self.controller = controller
        self.scene = QGraphicsScene()
        self.setScene(self.scene)

        self.padding_x = 16
        self.padding_y = 16

        self.spacing_x = 16
        self.spacing_y = 16

        self.current_columns = None

        self.items: List[ThumbFileItem] = []

        self.file_collection = None
        self.shared_pixmaps = None
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
        assert file_collection != self.file_collection
        logging.debug("ThumbView.set_file_collection")
        self.file_collection = file_collection
        self.file_collection.sig_files_set.connect(self.on_file_collection_set)
        self.file_collection.sig_files_reordered.connect(self.on_file_collection_reordered)
        self.file_collection.sig_files_filtered.connect(self.on_file_collection_filtered)
        self.on_file_collection_set()

    def on_file_collection_reordered(self):
        logging.debug("ThumbView.on_file_collection_reordered")
        fi2it = {item.fileinfo.abspath(): item for item in self.items}
        fileinfos = self.file_collection.get_fileinfos()
        try:
            self.items = [fi2it[fileinfo.abspath()] for fileinfo in fileinfos if not fileinfo.is_filtered]
        except:
            print("<<<<<<<<<")
            for k, v in fi2it.items():
                print(k)
            print("--------")
            for fi in fileinfos:
                print(fi.abspath())
            print(">>>>>>>>")
            raise
        self.layout_items()

    def on_file_collection_filtered(self):
        logging.debug("ThumbView.on_file_collection_filtered")

        for item in self.items:
            if not item.fileinfo.is_filtered:
                item.setVisible(True)
            else:
                item.setVisible(False)

        self.layout_items()

    def on_file_collection_set(self):
        logging.debug("ThumbView.on_file_collection_set")
        fileinfos = self.file_collection.get_fileinfos()

        self.scene.clear()
        self.items = []

        for fileinfo in fileinfos:
            thumb = None
            if not fileinfo.is_filtered:
                thumb = ThumbFileItem(fileinfo, self.controller, self)
            elif self.show_filtered:
                thumb = ThumbFileItem(fileinfo, self.controller, self)
                thumb.setOpacity(0.5)

            if thumb is not None:
                self.abspath2item[fileinfo.abspath()] = thumb
                self.scene.addItem(thumb)
                self.items.append(thumb)

        self.layout_items()

    def resizeEvent(self, ev):
        logging.debug("ThumbView.resizeEvent: %s", ev)
        super().resizeEvent(ev)
        self.layout_items(force=False)

    def layout_items_as_tiles(self, columns):
        visible_items = [item for item in self.items if not item.fileinfo.is_filtered]

        tile_w = self.tn_width
        tile_h = self.tn_height + 16

        right_x = 0
        bottom_y = 0

        col = 0
        row = 0
        for item in visible_items:
            x = col * (tile_w + self.spacing_x) + self.padding_x
            y = row * (tile_h + self.spacing_y) + self.padding_y

            right_x = max(right_x, x + tile_w + self.padding_x)
            bottom_y = y + tile_h + self.padding_y

            item.setPos(x, y)

            col += 1
            if col == columns:
                col = 0
                row += 1

        return right_x, bottom_y

    def layout_items(self, force=True):
        logging.debug("ThumbView.layout_items: %s", self.scene.itemIndexMethod())

        tile_w = self.tn_width
        tile_h = self.tn_height + 16

        columns = ((self.viewport().width() - 2 * self.padding_x + self.spacing_x) //
                   (tile_w + self.spacing_x))

        if columns == self.current_columns:
            logging.debug("ThumbView.layout_items: skipping, no relayout required")
            return
        else:
            self.current_columns = columns

        self.setUpdatesEnabled(False)
        old_item_index_method = self.scene.itemIndexMethod()
        self.scene.setItemIndexMethod(QGraphicsScene.NoIndex)

        right_x, bottom_y = self.layout_items_as_tiles(columns)

        if True:  # center alignment
            w = right_x
        else:
            w = max(self.viewport().size().width(), right_x)
        h = max(self.viewport().size().height(), bottom_y)

        bounding_rect = QRectF(0, 0, w, h)
        logging.debug("ThumbView.layout_items:done")
        self.setSceneRect(bounding_rect)

        logging.debug("ThumbView.layout_items: rebuilding BSP")
        self.scene.setItemIndexMethod(old_item_index_method)
        self.setUpdatesEnabled(True)

        logging.debug("ThumbView.layout_items: rebuilding BSP: Done")
        self.repaint()

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

        self.shared_pixmaps = SharedPixmaps(self.tn_size)
        self.reload()

    def pixmap_from_fileinfo(self, fileinfo, tn_size):
        tn_size = 3 * tn_size // 4

        if fileinfo.isdir():
            return self.shared_pixmaps.folder
        else:
            ext = fileinfo.ext()
            if ext == ".rar":
                return self.shared_pixmaps.rar
            elif ext == ".zip":
                return self.shared_pixmaps.zip
            elif ext == ".txt":
                return self.shared_pixmaps.txt
            else:
                return None

    def reload(self):
        for item in self.items:
            item.reload()
        self.layout_items()

    def receive_thumbnail(self, filename, flavor, thumbnail_filename, thumbnail_status):
        item = self.abspath2item.get(filename, None)
        if item is not None:
            item.reload_pixmap()
        else:
            print("MISSING!!!!!!!", filename)
            for k, v in self.abspath2item.items():
                print("->", k)

    def reload_thumbnails(self):
        for item in self.items:
            item.reload_thumbnail()


# EOF #
