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
from dirtools.fileview.file_info import FileInfo
from dirtools.fileview.thumb_file_item import ThumbFileItem
from dirtools.fileview.layouter import Layouter
from dirtools.fileview.layout import TileStyle
from dirtools.fileview.settings import settings
from dirtools.fileview.profiler import profile

logger = logging.getLogger(__name__)


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
        self.image = QPixmap(resource_filename("dirtools", "fileview/icons/noun_386758_cc.png"))  # noun_757280_cc.png
        self.loading = QPixmap(resource_filename("dirtools", "fileview/icons/noun_409399_cc.png"))
        self.error = QPixmap(resource_filename("dirtools", "fileview/icons/noun_175057_cc.png"))
        self.locked = QPixmap(resource_filename("dirtools", "fileview/icons/noun_236873_cc.png"))
        self.new = QPixmap(resource_filename("dirtools", "fileview/icons/noun_258297_cc.png"))


class FileViewStyle:

    def __init__(self):
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

        self.abspath2item: Dict[str, ThumbFileItem] = {}
        self.setAcceptDrops(True)

        self.scene = QGraphicsScene()
        self.setScene(self.scene)

        self.scene.selectionChanged.connect(self.on_selection_changed)

        self.style = FileViewStyle()

        self.tile_style = TileStyle()
        self.layouter = Layouter(self.scene, self.tile_style)
        self.layout = None

        self.items: List[ThumbFileItem] = []

        self.level_of_detail = 3
        self.zoom_index = 5

        self.file_collection = None

        self.needs_layout = True

        self.apply_zoom()
        self.cursor_item: Optional[ThumbFileItem] = None
        self.crop_thumbnails = False
        self.column_style = False
        self.setBackgroundBrush(QBrush(Qt.white, Qt.SolidPattern))
        self.resize_timer = None

        self.setDragMode(QGraphicsView.RubberBandDrag)

    def prepare(self):
        for item in self.items:
            item.prepare()

    def on_selection_changed(self):
        count = len(self.scene.selectedItems())
        print("{} files selected".format(count))

    def cursor_move(self, dx: int, dy: int):
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

    def keyPressEvent(self, ev):
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

    def set_crop_thumbnails(self, v):
        self.crop_thumbnails = v
        for item in self.items:
            item.update()

    def dragMoveEvent(self, ev):
        # the default implementation will check if any item in the
        # scene accept a drop event, we don't want that, so we
        # override the function to do nothing
        pass

    def dragEnterEvent(self, ev):
        print("dragEnterEvent", ev.mimeData().formats())
        if ev.mimeData().hasFormat("text/uri-list"):
            ev.accept()
        else:
            ev.ignore()

    def dragLeaveEvent(self, ev):
        print("dragLeaveEvent: leave")

    def dropEvent(self, ev):
        urls = ev.mimeData().urls()
        # [PyQt5.QtCore.QUrl('file:///home/ingo/projects/dirtool/trunk/setup.py')]
        self.controller.add_files([url.path() for url in urls])

    def set_file_collection(self, file_collection):
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

    def on_file_added(self, fileinfo: FileInfo):
        logger.debug("ThumbView.on_file_added: %s", fileinfo)
        item = ThumbFileItem(fileinfo, self.controller, self)
        item.new = True
        self.abspath2item[fileinfo.abspath()] = item
        self.scene.addItem(item)
        self.items.append(item)

        self.style_item(item)
        self.layouter.append_item(item)
        self.layouter.resize(self.viewport().width(), self.viewport().height())
        self.refresh_bounding_rect()

    def on_file_removed(self, abspath):
        logger.debug("ThumbView.on_file_removed: %s", abspath)
        item = self.abspath2item.get(abspath, None)
        if item is not None:
            self.scene.removeItem(item)
            del self.abspath2item[abspath]
            self.items.remove(item)
            self.layout_items()

    def on_file_changed(self, fileinfo):
        logger.debug("ThumbView.on_file_changed: %s", fileinfo)
        item = self.abspath2item.get(fileinfo.abspath(), None)
        if item is not None:
            item.new = True
            item.update()

    def on_file_updated(self, fileinfo):
        logger.debug("ThumbView.on_file_updated: %s", fileinfo)
        item = self.abspath2item.get(fileinfo.abspath(), None)
        if item is not None:
            item.update()

    def on_file_collection_reordered(self):
        logger.debug("ThumbView.on_file_collection_reordered")
        fi2it = {item.fileinfo.abspath(): item for item in self.items}
        fileinfos = self.file_collection.get_fileinfos()
        self.items = [fi2it[fileinfo.abspath()] for fileinfo in fileinfos if fileinfo.abspath() in fi2it]
        self.layout_items()

    def on_file_collection_filtered(self):
        logger.debug("ThumbView.on_file_collection_filtered")
        self.style_items()
        self.layout_items()

    def on_file_collection_grouped(self):
        logger.debug("ThumbView.on_file_collection_grouped")
        self.style_items()
        self.layout_items()

    def on_file_collection_set(self):
        logger.debug("ThumbView.on_file_collection_set")
        fileinfos = self.file_collection.get_fileinfos()

        self.items.clear()
        self.cursor_item = None
        self.abspath2item.clear()
        self.scene.clear()
        self.layout = None

        for fileinfo in fileinfos:
            item = ThumbFileItem(fileinfo, self.controller, self)
            self.abspath2item[fileinfo.abspath()] = item
            self.scene.addItem(item)
            self.items.append(item)

        self.style_items()
        self.layout_items()

    def resizeEvent(self, ev):
        logger.debug("ThumbView.resizeEvent: %s", ev)
        super().resizeEvent(ev)

        if settings.value("globals/resize_delay", True):
            if self.resize_timer is not None:
                self.killTimer(self.resize_timer)
            self.resize_timer = self.startTimer(100)
        else:
            self.layout_items()

    def timerEvent(self, ev):
        if ev.timerId() == self.resize_timer:
            self.killTimer(self.resize_timer)
            self.resize_timer = None

            if self.layout is not None:
                self.layout.resize(self.viewport().width(), self.viewport().height())
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

    def initPainter(self, painter):
        logger.debug("ThumbView.initPainter:")
        painter.setRenderHint(QPainter.SmoothPixmapTransform)
        painter.setRenderHint(QPainter.TextAntialiasing)
        painter.setRenderHint(QPainter.Antialiasing)

    @profile
    def paintEvent(self, ev):
        if self.needs_layout:
            self._layout_items()
            self.needs_layout = False

        super().paintEvent(ev)

    def layout_items(self):
        self.needs_layout = True
        self.invalidateScene()
        self.update()

    @profile
    def _layout_items(self):
        logger.debug("ThumbView._layout_items")

        self.setUpdatesEnabled(False)
        # old_item_index_method = self.scene.itemIndexMethod()
        # self.scene.setItemIndexMethod(QGraphicsScene.NoIndex)
        self.layouter.clear_appends()
        self.layout = self.layouter.build_layout(self.items)

        self.layout.resize(self.viewport().width(), self.viewport().height())
        self.refresh_bounding_rect()

        # self.scene.setItemIndexMethod(old_item_index_method)
        self.setUpdatesEnabled(True)

        logger.debug("ThumbView.layout_items: done")

    def refresh_bounding_rect(self):
        def get_bounding_rect():
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

    def zoom_in(self):
        self.zoom_index += 1
        if self.zoom_index > 11:
            self.zoom_index = 11
        self.apply_zoom()

    def zoom_out(self):
        self.zoom_index -= 1
        if self.zoom_index < 0:
            self.zoom_index = 0
        self.apply_zoom()

    def apply_zoom(self):
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

        self.style_items()
        self.layout_items()
        for item in self.items:
            item.update()

    def icon_from_fileinfo(self, fileinfo):
        mimetype = self.controller.app.mime_database.get_mime_type(fileinfo.abspath())
        return self.controller.app.mime_database.get_icon_from_mime_type(mimetype)

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

    def scroll_by(self, x, y):
        scrollbar = self.verticalScrollBar()
        scrollbar.setValue(scrollbar.value() + y)

        scrollbar = self.horizontalScrollBar()
        scrollbar.setValue(scrollbar.value() + x)

    def set_cursor_to_fileinfo(self, fileinfo):
        if self.cursor_item is not None:
            self.cursor_item.update()
        self.cursor_item = self.abspath2item.get(fileinfo.abspath(), None)
        if self.cursor_item is not None:
            self.cursor_item.update()

    def contextMenuEvent(self, ev):
        if ev.reason() == QContextMenuEvent.Keyboard:
            if self.cursor_item is None:
                self.controller.on_context_menu(ev)
            else:
                self.controller.on_item_context_menu(ev, self.cursor_item)
        else:
            super().contextMenuEvent(ev)
            if not ev.isAccepted():
                self.controller.on_context_menu(ev)


# EOF #
