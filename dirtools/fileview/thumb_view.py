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


from typing import List, Dict, Optional

import logging

from pkg_resources import resource_filename

from PyQt5.QtCore import Qt, QRectF
from PyQt5.QtGui import (QBrush, QIcon, QColor, QPixmap, QPainter,
                         QFontMetrics, QFont, QContextMenuEvent)
from PyQt5.QtWidgets import QGraphicsView, QGraphicsScene

from dirtools.dbus_thumbnailer import DBusThumbnailerError
from dirtools.fileview.file_collection import FileCollection
from dirtools.fileview.file_info import FileInfo
from dirtools.fileview.layout import RootLayout, TileStyle
from dirtools.fileview.layout_builder import LayoutBuilder
from dirtools.fileview.location import Location
from dirtools.fileview.profiler import profile
from dirtools.fileview.settings import settings
from dirtools.fileview.thumb_file_item import ThumbFileItem

logger = logging.getLogger(__name__)


class SharedIcons:

    def __init__(self) -> None:
        self.folder = QIcon.fromTheme("folder")
        self.rar = QIcon.fromTheme("rar")
        self.zip = QIcon.fromTheme("zip")
        self.txt = QIcon.fromTheme("txt")
        self.image_loading = QIcon.fromTheme("image-loading")
        self.image_missing = QIcon.fromTheme("image-missing")
        self.locked = QIcon.fromTheme("locked")


class SharedPixmaps:

    def __init__(self) -> None:
        self.video = QPixmap(resource_filename("dirtools", "fileview/icons/noun_36746_cc.png"))
        self.image = QPixmap(resource_filename("dirtools", "fileview/icons/noun_386758_cc.png"))  # noun_757280_cc.png
        self.loading = QPixmap(resource_filename("dirtools", "fileview/icons/noun_409399_cc.png"))
        self.error = QPixmap(resource_filename("dirtools", "fileview/icons/noun_175057_cc.png"))
        self.locked = QPixmap(resource_filename("dirtools", "fileview/icons/noun_236873_cc.png"))
        self.new = QPixmap(resource_filename("dirtools", "fileview/icons/noun_258297_cc.png"))


class FileViewStyle:

    def __init__(self) -> None:
        self.font = QFont("Verdana", 8)
        self.fm = QFontMetrics(self.font)
        self.shared_icons = SharedIcons()
        self.shared_pixmaps = SharedPixmaps()


class ThumbView(QGraphicsView):

    def __init__(self, controller) -> None:
        super().__init__()

        self.controller = controller

        self.setCacheMode(QGraphicsView.CacheBackground)

        self.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        # self.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOn)

        self.show_filtered = False

        self.location2item: Dict[Location, ThumbFileItem] = {}
        self.setAcceptDrops(True)

        self.scene = QGraphicsScene()
        self.setScene(self.scene)

        self.scene.selectionChanged.connect(self.on_selection_changed)

        self.style = FileViewStyle()

        self.tile_style = TileStyle()

        self.layout: Optional[RootLayout] = None
        self.layout_builder = LayoutBuilder(self.scene, self.tile_style)

        self.items: List[ThumbFileItem] = []

        self.level_of_detail = 3
        self.zoom_index = 5

        self.file_collection: Optional[FileCollection] = None

        self.needs_layout = True

        self.apply_zoom()
        self.cursor_item: Optional[ThumbFileItem] = None
        self.crop_thumbnails = False
        self.column_style = False
        self.setBackgroundBrush(QBrush(Qt.white, Qt.SolidPattern))
        self.resize_timer = None

        self.setDragMode(QGraphicsView.RubberBandDrag)

        self.setRenderHints(QPainter.SmoothPixmapTransform |
                            QPainter.TextAntialiasing |
                            QPainter.Antialiasing)

    def prepare(self) -> None:
        for item in self.items:
            item.prepare()

    def on_selection_changed(self) -> None:
        count = len(self.scene.selectedItems())
        print("{} files selected".format(count))

    def cursor_move(self, dx: int, dy: int) -> None:
        def best_item(items, rect):
            """Select the most top/left and fully visible item"""
            def contains(item):
                r = QRectF(item.tile_rect)
                r.moveTo(item.pos())
                return rect.contains(r)

            items = sorted(items, key=lambda item: (not contains(item),
                                                    item.pos().x(),
                                                    item.pos().y()))
            return items[0]

        if self.cursor_item is None:
            rect = self.mapToScene(self.rect()).boundingRect()
            items = [item for item in self.scene.items(rect) if isinstance(item, ThumbFileItem)]
            if not items:
                return
            else:
                self.cursor_item = best_item(items, rect)
                self.ensureVisible(self.cursor_item)
                self.cursor_item.update()
                return

        self.cursor_item.update()

        # query a rectengular area next to the current item for items,
        # use the first one that we find
        rect = QRectF(self.cursor_item.tile_rect)
        rect.moveTo(self.cursor_item.pos().x() + (self.cursor_item.tile_rect.width() + 4) * dx,
                    self.cursor_item.pos().y() + (self.cursor_item.tile_rect.height() + 4) * dy)
        items = [item for item in self.scene.items(rect) if isinstance(item, ThumbFileItem)]
        if items:
            self.cursor_item = items[0]

        self.cursor_item.update()
        self.ensureVisible(self.cursor_item)

    def keyPressEvent(self, ev) -> None:
        if ev.key() == Qt.Key_Escape:
            self.scene.clearSelection()
            item = self.cursor_item
            self.cursor_item = None
            if item is not None:
                item.update()
        elif ev.key() == Qt.Key_Space and ev.modifiers() & Qt.ControlModifier:
            if self.cursor_item is not None:
                self.cursor_item.setSelected(not self.cursor_item.isSelected())
        elif ev.key() == Qt.Key_Left:
            if self.cursor_item is not None and ev.modifiers() & Qt.ShiftModifier:
                self.cursor_item.setSelected(True)
            self.cursor_move(-1, 0)
        elif ev.key() == Qt.Key_Right:
            if self.cursor_item is not None and ev.modifiers() & Qt.ShiftModifier:
                self.cursor_item.setSelected(True)
            self.cursor_move(+1, 0)
        elif ev.key() == Qt.Key_Up:
            if self.cursor_item is not None and ev.modifiers() & Qt.ShiftModifier:
                self.cursor_item.setSelected(True)
            self.cursor_move(0, -1)
        elif ev.key() == Qt.Key_Down:
            if self.cursor_item is not None and ev.modifiers() & Qt.ShiftModifier:
                self.cursor_item.setSelected(True)
            self.cursor_move(0, +1)
        elif ev.key() == Qt.Key_Return:
            if self.cursor_item is not None:
                self.cursor_item.click_action()
        else:
            super().keyPressEvent(ev)

    def set_crop_thumbnails(self, v) -> None:
        self.crop_thumbnails = v
        for item in self.items:
            item.update()

    def dragMoveEvent(self, ev) -> None:
        # the default implementation will check if any item in the
        # scene accept a drop event, we don't want that, so we
        # override the function to do nothing
        pass

    def dragEnterEvent(self, ev) -> None:
        print("dragEnterEvent", ev.mimeData().formats())
        if ev.mimeData().hasFormat("text/uri-list"):
            ev.accept()
        else:
            ev.ignore()

    def dragLeaveEvent(self, ev) -> None:
        print("dragLeaveEvent: leave")

    def dropEvent(self, ev):
        urls = ev.mimeData().urls()
        # [PyQt5.QtCore.QUrl('file:///home/ingo/projects/dirtool/trunk/setup.py')]
        self.controller.add_files([Location.from_url(url.toString()) for url in urls])

    def set_file_collection(self, file_collection: FileCollection) -> None:
        assert file_collection != self.file_collection
        logger.debug("ThumbView.set_file_collection")
        self.file_collection = file_collection
        self.file_collection.sig_files_set.connect(self.on_file_collection_set)
        self.file_collection.sig_files_reordered.connect(self.on_file_collection_reordered)
        self.file_collection.sig_files_filtered.connect(self.on_file_collection_filtered)
        self.file_collection.sig_files_grouped.connect(self.on_file_collection_grouped)

        self.file_collection.sig_file_added.connect(self.on_file_added)
        self.file_collection.sig_file_removed.connect(self.on_file_removed)
        self.file_collection.sig_file_changed.connect(self.on_file_changed)
        self.file_collection.sig_file_updated.connect(self.on_file_updated)

        self.on_file_collection_set()

    def on_file_added(self, fileinfo: FileInfo) -> None:
        logger.debug("ThumbView.on_file_added: %s", fileinfo)
        item = ThumbFileItem(fileinfo, self.controller, self)
        item.new = True
        self.location2item[fileinfo.location()] = item
        self.scene.addItem(item)
        self.items.append(item)

        self.style_item(item)

        if self.layout is not None:
            self.layout.append_item(item)
            self.refresh_bounding_rect()

    def on_file_removed(self, location: Location) -> None:
        logger.debug("ThumbView.on_file_removed: %s", location)
        item = self.location2item.get(location, None)
        if item is not None:
            self.scene.removeItem(item)
            del self.location2item[location]
            self.items.remove(item)
            self.layout_items()

    def on_file_changed(self, fileinfo: FileInfo) -> None:
        logger.debug("ThumbView.on_file_changed: %s", fileinfo)
        item = self.location2item.get(fileinfo.location(), None)
        if item is not None:
            item.new = True
            item.update()

    def on_file_updated(self, fileinfo: FileInfo) -> None:
        logger.debug("ThumbView.on_file_updated: %s", fileinfo)
        item = self.location2item.get(fileinfo.location(), None)
        if item is not None:
            item.update()

    def on_file_collection_reordered(self) -> None:
        logger.debug("ThumbView.on_file_collection_reordered")
        fi2it = {item.fileinfo.location(): item for item in self.items}
        fileinfos = self.file_collection.get_fileinfos()
        self.items = [fi2it[fileinfo.location()] for fileinfo in fileinfos if fileinfo.location() in fi2it]
        self.layout_items()

    def on_file_collection_filtered(self) -> None:
        logger.debug("ThumbView.on_file_collection_filtered")
        self.style_items()
        self.layout_items()

    def on_file_collection_grouped(self) -> None:
        logger.debug("ThumbView.on_file_collection_grouped")
        self.style_items()
        self.layout_items()

    def clear(self) -> None:
        self.items.clear()
        self.cursor_item = None
        self.location2item.clear()
        self.scene.clear()
        self.layout = None

    def on_file_collection_set(self) -> None:
        logger.debug("ThumbView.on_file_collection_set")
        self.clear()

        fileinfos = self.file_collection.get_fileinfos()

        for fileinfo in fileinfos:
            item = ThumbFileItem(fileinfo, self.controller, self)
            self.location2item[fileinfo.location()] = item
            self.scene.addItem(item)
            self.items.append(item)

        self.style_items()
        self.layout_items()

    def resizeEvent(self, ev) -> None:
        logger.debug("ThumbView.resizeEvent: %s", ev)
        super().resizeEvent(ev)

        if settings.value("globals/resize_delay", True):
            if self.resize_timer is not None:
                self.killTimer(self.resize_timer)
            self.resize_timer = self.startTimer(100)
        else:
            self.layout_items()

    def timerEvent(self, ev) -> None:
        if ev.timerId() == self.resize_timer:
            self.killTimer(self.resize_timer)
            self.resize_timer = None

            if self.layout is not None:
                self.layout.layout(self.viewport().width())
            self.layout_items()
        else:
            assert False, "timer foobar"

    def style_item(self, item) -> None:
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

    def style_items(self) -> None:
        for item in self.items:
            self.style_item(item)

    def initPainter(self, painter):
        # logger.debug("ThumbView.initPainter:")
        pass

    @profile
    def paintEvent(self, ev) -> None:
        if self.needs_layout:
            self._layout_items()
            self.needs_layout = False

        super().paintEvent(ev)

    def layout_items(self):
        self.needs_layout = True
        self.invalidateScene()
        self.update()

    @profile
    def _layout_items(self) -> None:
        logger.debug("ThumbView._layout_items")

        self.setUpdatesEnabled(False)
        # old_item_index_method = self.scene.itemIndexMethod()
        # self.scene.setItemIndexMethod(QGraphicsScene.NoIndex)
        self.layout = self.layout_builder.build_layout(self.items)

        self.layout.layout(self.viewport().width())
        self.refresh_bounding_rect()

        # self.scene.setItemIndexMethod(old_item_index_method)
        self.setUpdatesEnabled(True)

        logger.debug("ThumbView.layout_items: done")

    def refresh_bounding_rect(self) -> None:
        if self.layout is None:
            return

        def get_bounding_rect() -> QRectF:
            rect = self.layout.get_bounding_rect()

            if True:  # self.layout_style == LayoutStyle.ROWS:
                # center alignment
                w = rect.right()
            else:
                # top/center alignment
                w = max(self.viewport().width(), rect.right())
            h = max(self.viewport().height(), rect.bottom())

            return QRectF(0, 0, w, h)

        self.setSceneRect(get_bounding_rect())

    def zoom_in(self) -> None:
        self.zoom_index += 1
        if self.zoom_index > 11:
            self.zoom_index = 11
        self.apply_zoom()

    def zoom_out(self) -> None:
        self.zoom_index -= 1
        if self.zoom_index < 0:
            self.zoom_index = 0
        self.apply_zoom()

    def apply_zoom(self) -> None:
        if self.zoom_index == 0:
            self.tn_width = 256
            self.tn_height = 16
            self.tn_size = min(self.tn_width, self.tn_height)

            self.column_style = True

            self.tile_style.set_arrangement(TileStyle.Arrangement.COLUMNS)
            self.tile_style.set_padding(8, 8)
            self.tile_style.set_spacing(16, 8)
            self.tile_style.set_tile_size(self.tn_width, self.tn_height)
        else:
            self.tn_width = [16, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536][self.zoom_index]
            self.tn_height = self.tn_width
            self.tn_size = min(self.tn_width, self.tn_height)

            self.column_style = False
            self.tile_style.set_arrangement(TileStyle.Arrangement.ROWS)
            self.tile_style.set_padding(16, 16)
            self.tile_style.set_spacing(16, 16)
            k = [0, 1, 1, 2, 3][self.level_of_detail]
            self.tile_style.set_tile_size(self.tn_width, self.tn_height + 16 * k)

        if self.zoom_index < 2:
            self.flavor = "normal"
        else:
            self.flavor = "large"

        for item in self.items:
            item.set_tile_size(self.tile_style.tile_width, self.tile_style.tile_height)

        self.style_items()
        self.layout_items()

        for item in self.items:
            item.update()

    def icon_from_fileinfo(self, fileinfo: FileInfo) -> QIcon:
        mimetype = self.controller.app.mime_database.get_mime_type(fileinfo.location())
        return self.controller.app.mime_database.get_icon_from_mime_type(mimetype)

    def receive_thumbnail(self, location: Location, flavor: str,
                          pixmap: 'QPixmap', error_code: int, message: str) -> None:
        item = self.location2item.get(location, None)
        if item is not None:
            self.receive_thumbnail_for_item(item, flavor, pixmap, error_code, message)
            item.set_thumbnail_pixmap(pixmap, flavor)
        else:
            # receiving thumbnail for item that no longer exists, this
            # is normal when switching directories quickly
            pass

    def receive_thumbnail_for_item(self, item, flavor: str, pixmap: QPixmap, error_code: int, message: str) -> None:
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

    def scroll_by(self, x, y):
        scrollbar = self.verticalScrollBar()
        scrollbar.setValue(scrollbar.value() + y)

        scrollbar = self.horizontalScrollBar()
        scrollbar.setValue(scrollbar.value() + x)

    def set_cursor_to_fileinfo(self, fileinfo):
        if self.cursor_item is not None:
            self.cursor_item.update()
        self.cursor_item = self.location2item.get(fileinfo.location(), None)
        if self.cursor_item is not None:
            self.cursor_item.update()

    def contextMenuEvent(self, ev):
        if ev.reason() == QContextMenuEvent.Keyboard:
            if self.cursor_item is None:
                self.controller.on_context_menu(ev.globalPos())
            else:
                self.controller.on_item_context_menu(ev, self.cursor_item)
        else:
            super().contextMenuEvent(ev)
            if not ev.isAccepted():
                self.controller.on_context_menu(ev.globalPos())


# EOF #
