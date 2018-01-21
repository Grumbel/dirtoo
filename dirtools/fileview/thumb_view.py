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

from PyQt5.QtCore import Qt, QThreadPool, QRunnable, QEventLoop, pyqtSignal
from PyQt5.QtGui import QBrush, QIcon, QPixmap
from PyQt5.QtWidgets import (
    QGraphicsView,
    QGraphicsScene,
)

from dirtools.fileview.thumb_file_item import ThumbFileItem
from dirtools.fileview.tile_layouter import TileLayouter
from dirtools.fileview.profiler import Profiler, profile

class ThumbnailLoader(QRunnable):

    def __init__(self, thumb_view, item, filename):
        super().__init__()
        self.setAutoDelete(True)

        self.thumb_view = thumb_view
        self.item = item
        self.filename = filename

    def run(self):
        pixmap = QPixmap(self.filename)

        if pixmap.isNull():
            logging.error("ThumbnailLoader.run: failed to load %s", self.filename)
            pixmap = self.thumb_view.shared_pixmaps.image_missing
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

        self.thumb_view.sig_thumbnail_ready.emit(self.item, pixmap)


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

    sig_thumbnail_ready = pyqtSignal(ThumbFileItem, QPixmap)

    def __init__(self, controller):
        super().__init__()

        self.show_filtered = False

        self.abspath2item: Dict[str, ThumbFileItem] = {}
        self.setAcceptDrops(True)

        self.controller = controller
        self.scene = QGraphicsScene()
        self.setScene(self.scene)

        self.layouter = TileLayouter()

        self.items: List[ThumbFileItem] = []

        self.level_of_detail = 1

        self.file_collection = None
        self.shared_pixmaps = None
        self.zoom_index = 2
        self.apply_zoom()

        self.sig_thumbnail_ready.connect(self.thumbnail_ready)

        self.thread_pool = QThreadPool()

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
        self.items = [fi2it[fileinfo.abspath()] for fileinfo in fileinfos if fileinfo.abspath() in fi2it]
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
        self.layouter.resize(ev.size().width(), ev.size().height())
        self.layout_items(force=False)

    def layout_items(self, force=True):
        logging.debug("ThumbView.layout_items")

        self.setUpdatesEnabled(False)
        # old_item_index_method = self.scene.itemIndexMethod()
        # self.scene.setItemIndexMethod(QGraphicsScene.NoIndex)

        visible_items = [item for item in self.items if not item.fileinfo.is_filtered]

        self.layouter.layout(visible_items, force=force)
        self.setSceneRect(self.layouter.get_bounding_rect())

        # self.scene.setItemIndexMethod(old_item_index_method)
        self.setUpdatesEnabled(True)

        logging.debug("ThumbView.layout_items: done")

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

        self.layouter.set_tile_size(self.tn_width, self.tn_height + 16 * self.level_of_detail)

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

    @profile
    def reload(self):
        for item in self.items:
            item.reload()
        self.layout_items()

    def receive_thumbnail(self, filename, flavor, thumbnail_filename, thumbnail_status):
        item = self.abspath2item.get(filename, None)
        if item is not None:
            item.reload_pixmap(thumbnail_filename)
        else:
            print("MISSING!!!!!!!", filename)
            for k, v in self.abspath2item.items():
                print("->", k)

    def thumbnail_ready(self, item, pixmap):
        # print("thumbnail_ready:", pixmap)
        item.set_pixmap(pixmap)

    def thumbnail_request(self, item, filename):
        # print("thumbnail_request:", filename)
        self.thread_pool.start(ThumbnailLoader(self, item, filename))

    def reload_thumbnails(self):
        for item in self.items:
            item.reload_thumbnail()

    def less_details(self):
        self.level_of_detail -= 1
        if self.level_of_detail < 0:
            self.level_of_detail = 0
        self.apply_zoom()

    def more_details(self):
        self.level_of_detail += 1
        if self.level_of_detail > 4:
            self.level_of_detail = 4
        self.apply_zoom()


# EOF #
