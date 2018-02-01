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
from pkg_resources import resource_filename

from PyQt5.QtCore import Qt, QThreadPool
from PyQt5.QtGui import QBrush, QIcon, QColor, QPixmap
from PyQt5.QtWidgets import (
    QGraphicsView,
    QGraphicsScene,
)

from dirtools.fileview.thumb_file_item import ThumbFileItem
from dirtools.fileview.tile_layouter import TileLayouter
from dirtools.fileview.profiler import profile
from dirtools.dbus_thumbnailer import DBusThumbnailerError


class SharedIcons:

    def __init__(self):
        self.folder = QIcon.fromTheme("folder")
        self.rar = QIcon.fromTheme("rar")
        self.zip = QIcon.fromTheme("zip")
        self.txt = QIcon.fromTheme("txt")
        self.image_loading = QIcon.fromTheme("image-loading")
        self.image_missing = QIcon.fromTheme("image-missing")
        self.locked = QIcon.fromTheme("locked")


class SharedPixmaps:

    def __init__(self):
        self.video = QPixmap(resource_filename("dirtools", "fileview/icons/noun_36746_cc.png"))
        self.image = QPixmap(resource_filename("dirtools", "fileview/icons/noun_386758_cc.png"))  # noun_757280_cc.png"))
        self.loading = QPixmap(resource_filename("dirtools", "fileview/icons/noun_409399_cc.png"))


class ThumbView(QGraphicsView):

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

        self.shared_icons = SharedIcons()
        self.shared_pixmaps = SharedPixmaps()

        self.zoom_index = 2
        self.apply_zoom()

        self.crop_thumbnails = False

        self.setBackgroundBrush(QBrush(Qt.white, Qt.SolidPattern))

        self.resize_timer = None

    def set_crop_thumbnails(self, v):
        self.crop_thumbnails = v
        for item in self.items:
            item.update()

    def dragMoveEvent(self, e):
        # the default implementation will check if any item in the
        # scene accept a drop event, we don't want that, so we
        # override the function to do nothing
        pass

    def dragEnterEvent(self, e):
        print("dragEnterEvent", e.mimeData().formats())
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

        self.file_collection.sig_file_added.connect(self.on_file_added)
        self.file_collection.sig_file_removed.connect(self.on_file_removed)

        self.on_file_collection_set()

    def on_file_added(self, fileinfo):
        thumb = ThumbFileItem(fileinfo, self.controller, self)
        self.abspath2item[fileinfo.abspath()] = thumb
        self.scene.addItem(thumb)
        self.items.append(thumb)

        self.style_item(thumb)
        self.layouter.layout_item(thumb)
        self.setSceneRect(self.layouter.get_bounding_rect())

    def on_file_removed(self, abspath):
        item = self.abspath2item[abspath]
        self.scene.removeItem(item)
        del self.abspath2item[abspath]
        self.items.remove(item)
        self.layout_items()

    def on_file_collection_reordered(self):
        logging.debug("ThumbView.on_file_collection_reordered")
        fi2it = {item.fileinfo.abspath(): item for item in self.items}
        fileinfos = self.file_collection.get_fileinfos()
        self.items = [fi2it[fileinfo.abspath()] for fileinfo in fileinfos if fileinfo.abspath() in fi2it]
        self.layout_items()

    def on_file_collection_filtered(self):
        logging.debug("ThumbView.on_file_collection_filtered")
        self.style_items()
        self.layout_items()

    def on_file_collection_set(self):
        logging.debug("ThumbView.on_file_collection_set")
        fileinfos = self.file_collection.get_fileinfos()

        self.items.clear()
        self.abspath2item.clear()
        self.scene.clear()

        for fileinfo in fileinfos:
            thumb = ThumbFileItem(fileinfo, self.controller, self)
            self.abspath2item[fileinfo.abspath()] = thumb
            self.scene.addItem(thumb)
            self.items.append(thumb)

        self.style_items()
        self.layout_items()

    def resizeEvent(self, ev):
        logging.debug("ThumbView.resizeEvent: %s", ev)
        super().resizeEvent(ev)

        if self.resize_timer is not None:
            self.killTimer(self.resize_timer)

        self.resize_timer = self.startTimer(100)

    def timerEvent(self, ev):
        if ev.timerId() == self.resize_timer:
            self.killTimer(self.resize_timer)
            self.resize_timer = None

            self.layouter.resize(self.size().width(), self.size().height())
            self.layout_items()
        else:
            assert False, "timer foobar"

    def style_item(self, item):
        if self.show_filtered:
            if item.fileinfo.is_hidden:
                item.setVisible(False)
            elif item.fileinfo.is_excluded:
                item.setVisible(True)
                item.setOpacity(0.5)
            else:
                item.setVisible(True)
                item.setOpacity(1.0)
        else:
            item.setVisible(item.fileinfo.is_visible)
            item.setOpacity(1.0)

    def style_items(self):
        for item in self.items:
            self.style_item(item)

    def layout_items(self, force=True):
        logging.debug("ThumbView.layout_items")

        self.setUpdatesEnabled(False)
        # old_item_index_method = self.scene.itemIndexMethod()
        # self.scene.setItemIndexMethod(QGraphicsScene.NoIndex)

        if self.show_filtered:
            visible_items = [item for item in self.items if not item.fileinfo.is_hidden]
        else:
            visible_items = [item for item in self.items if item.fileinfo.is_visible]

        self.layouter.layout(visible_items, force=force)
        self.setSceneRect(self.layouter.get_bounding_rect())

        # self.scene.setItemIndexMethod(old_item_index_method)
        self.setUpdatesEnabled(True)

        logging.debug("ThumbView.layout_items: done")

    def zoom_in(self):
        self.zoom_index += 1
        if self.zoom_index > 6:
            self.zoom_index = 6
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

        if self.zoom_index < 2:
            self.flavor = "normal"
        else:
            self.flavor = "large"

        self.reload()

    def icon_from_fileinfo(self, fileinfo):
        mime_type = self.controller.app.mime_database.get_mime_type(fileinfo.abspath())
        icon = QIcon.fromTheme(mime_type.iconName())
        if icon.isNull():
            icon = QIcon.fromTheme("application-octet-stream")
            assert not icon.isNull()
            return icon
        else:
            return icon

    @profile
    def reload(self):
        for item in self.items:
            item.reload()
        self.style_items()
        self.layout_items()

    def receive_thumbnail(self, filename, flavor, pixmap, error_code, message):
        item = self.abspath2item.get(filename, None)
        if item is not None:
            self.receive_thumbnail_for_item(item, flavor, pixmap, error_code, message)
            item.set_thumbnail_pixmap(pixmap, flavor)
        else:
            # receiving thumbnail for item that no longer exists, this
            # is normal when switching directories quickly
            pass

    def receive_thumbnail_for_item(self, item, flavor, pixmap, error_code, message):
        if pixmap is not None:
            item.set_thumbnail_pixmap(pixmap, flavor)
        else:
            if error_code is None:
                # thumbnail was generated, but couldn't be loaded
                item.set_thumbnail_pixmap(None, flavor)
            elif error_code == DBusThumbnailerError.UNSUPPORTED_MIMETYPE:
                pass
            elif error_code == DBusThumbnailerError.CONNECTION_FAILURE:
                pass
            elif error_code == DBusThumbnailerError.INVALID_DATA:
                pass
            elif error_code == DBusThumbnailerError.THUMBNAIL_RECURSION:
                pass
            elif error_code == DBusThumbnailerError.SAVE_FAILURE:
                pass
            elif error_code == DBusThumbnailerError.UNSUPPORTED_FLAVOR:
                pass

    def request_thumbnail(self, item, fileinfo, flavor):
        self.controller.request_thumbnail(fileinfo, flavor)

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

    def set_show_filtered(self, show_filtered):
        self.show_filtered = show_filtered
        self.style_items()
        self.layout_items()

    def set_filtered(self, filtered):
        if filtered:
            self.setBackgroundBrush(QBrush(QColor(220, 220, 255), Qt.SolidPattern))
        else:
            self.setBackgroundBrush(QBrush())


# EOF #
