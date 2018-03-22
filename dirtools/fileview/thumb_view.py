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


from typing import List, Dict, Optional, Set

import logging

from collections import defaultdict
from pkg_resources import resource_filename

from PyQt5.QtCore import Qt, QRectF
from PyQt5.QtGui import (QBrush, QIcon, QColor, QPixmap, QImage,
                         QPainter, QFontMetrics, QFont, QKeySequence,
                         QContextMenuEvent)
from PyQt5.QtWidgets import QGraphicsView, QShortcut

from dirtools.dbus_thumbnailer import DBusThumbnailerError
from dirtools.fileview.file_collection import FileCollection
from dirtools.fileview.file_info import FileInfo
from dirtools.fileview.layout import RootLayout
from dirtools.fileview.layout_builder import LayoutBuilder
from dirtools.fileview.location import Location
from dirtools.fileview.profiler import profile
from dirtools.fileview.settings import settings
from dirtools.fileview.mode import Mode, IconMode, ListMode, DetailMode, FileItemStyle
from dirtools.fileview.thumb_file_item import ThumbFileItem
from dirtools.fileview.file_graphics_scene import FileGraphicsScene

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
        self.font = QFont("Verdana", 10)
        self.fm = QFontMetrics(self.font)
        self.shared_icons = SharedIcons()
        self.shared_pixmaps = SharedPixmaps()


class ThumbView(QGraphicsView):

    def __init__(self, controller) -> None:
        super().__init__()

        self._controller = controller

        self.setCacheMode(QGraphicsView.CacheBackground)

        self.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        # self.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOn)

        self._show_filtered = False

        self._location2item: Dict[Location, List[ThumbFileItem]] = defaultdict(list)
        self.setAcceptDrops(True)

        self._scene = FileGraphicsScene()
        self.setScene(self._scene)

        self._scene.selectionChanged.connect(self.on_selection_changed)

        self._style = FileViewStyle()

        self._modes: List[Mode] = [
            IconMode(self),
            ListMode(self),
            DetailMode(self)
        ]
        self._mode = self._modes[FileItemStyle.ICON.value]

        self._layout: Optional[RootLayout] = None

        self._items: List[ThumbFileItem] = []

        self._file_collection: Optional[FileCollection] = None

        self._needs_layout = True

        self.apply_zoom()
        self._cursor_item: Optional[ThumbFileItem] = None
        self._crop_thumbnails = False
        self.setBackgroundBrush(QBrush(Qt.white, Qt.SolidPattern))
        self._resize_timer: Optional[int] = None

        self.setDragMode(QGraphicsView.RubberBandDrag)

        self.setRenderHints(QPainter.SmoothPixmapTransform |
                            QPainter.TextAntialiasing |
                            QPainter.Antialiasing)

        shortcut = QShortcut(QKeySequence(Qt.CTRL + Qt.Key_G), self)
        shortcut.setContext(Qt.WidgetShortcut)
        shortcut.activated.connect(self._on_reset)

        shortcut = QShortcut(QKeySequence(Qt.Key_Slash), self)
        shortcut.setContext(Qt.WidgetShortcut)
        shortcut.activated.connect(lambda: self._controller.show_location_toolbar(False))

    def _on_reset(self):
        self._controller.hide_all()

    def prepare(self) -> None:
        for item in self._items:
            item.prepare()

    def on_selection_changed(self) -> None:
        self._controller._update_info()

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

        if self._cursor_item is None:
            rect = self.mapToScene(self.rect()).boundingRect()
            items = [item for item in self._scene.items(rect) if isinstance(item, ThumbFileItem)]
            if not items:
                return
            else:
                self._cursor_item = best_item(items, rect)
                self.ensureVisible(self._cursor_item)
                self._cursor_item.update()
                return

        self._cursor_item.update()

        # query a rectengular area next to the current item for items,
        # use the first one that we find
        rect = QRectF(self._cursor_item.tile_rect)
        rect.moveTo(self._cursor_item.pos().x() + (self._cursor_item.tile_rect.width() + 4) * dx,
                    self._cursor_item.pos().y() + (self._cursor_item.tile_rect.height() + 4) * dy)
        items = [item for item in self._scene.items(rect) if isinstance(item, ThumbFileItem)]
        if items:
            self._cursor_item = items[0]

        self._cursor_item.update()
        self.ensureVisible(self._cursor_item)

    def keyPressEvent(self, ev) -> None:
        if ev.key() == Qt.Key_Escape:
            self._scene.clearSelection()
            item = self._cursor_item
            self._cursor_item = None
            if item is not None:
                item.update()
        elif ev.key() == Qt.Key_Space and ev.modifiers() & Qt.ControlModifier:
            if self._cursor_item is not None:
                self._cursor_item.setSelected(not self._cursor_item.isSelected())
        elif ev.key() == Qt.Key_Left:
            if self._cursor_item is not None and ev.modifiers() & Qt.ShiftModifier:
                self._cursor_item.setSelected(True)
            self.cursor_move(-1, 0)
        elif ev.key() == Qt.Key_Right:
            if self._cursor_item is not None and ev.modifiers() & Qt.ShiftModifier:
                self._cursor_item.setSelected(True)
            self.cursor_move(+1, 0)
        elif ev.key() == Qt.Key_Up:
            if self._cursor_item is not None and ev.modifiers() & Qt.ShiftModifier:
                self._cursor_item.setSelected(True)
            self.cursor_move(0, -1)
        elif ev.key() == Qt.Key_Down:
            if self._cursor_item is not None and ev.modifiers() & Qt.ShiftModifier:
                self._cursor_item.setSelected(True)
            self.cursor_move(0, +1)
        elif ev.key() == Qt.Key_Return:
            if self._cursor_item is not None:
                self._cursor_item.click_action(new_window=ev.modifiers() & Qt.ShiftModifier)
        else:
            super().keyPressEvent(ev)

    def set_crop_thumbnails(self, v) -> None:
        self._crop_thumbnails = v
        for item in self._items:
            item.update()

    def dragEnterEvent(self, ev) -> None:
        super().dragEnterEvent(ev)

        if not ev.isAccepted():
            print("ThumbView.dragEnterEvent()")

            if False:
                data = ev.mimeData()
                for fmt in data.formats():
                    print("Format:", fmt)
                    print(data.data(fmt))
                    print()
                print()

            if ev.mimeData().hasUrls():
                ev.acceptProposedAction()
                ev.accept()
            else:
                ev.ignore()

    def dragMoveEvent(self, ev) -> None:
        super().dragMoveEvent(ev)

        if not ev.isAccepted():
            print("ThumbView.dragMoveEvent()")
            # the default implementation will check if any item in the
            # scene accept a drop event, we don't want that, so we
            # override the function to do nothing for now
            ev.acceptProposedAction()
            pass

    def dragLeaveEvent(self, ev) -> None:
        super().dragLeaveEvent(ev)
        print("ThumbView.dragLeaveEvent()")

        if not ev.isAccepted():
            print("ThumbView.dragLeaveEvent()")

    def dropEvent(self, ev):
        super().dropEvent(ev)

        if not ev.isAccepted():
            print("ThumbView.dropEvent()")

            mime_data = ev.mimeData()
            assert mime_data.hasUrls()

            urls = mime_data.urls()
            files = [url.toLocalFile() for url in urls]
            action = ev.proposedAction()

            if action == Qt.CopyAction:
                print("copy", " ".join(files))
            elif action == Qt.MoveAction:
                print("move", " ".join(files))
            elif action == Qt.LinkAction:
                print("link", " ".join(files))
            else:
                print("unsupported drop action", action)

            # self._controller.add_files([Location.from_url(url.toString()) for url in urls])

    def set_file_collection(self, file_collection: FileCollection) -> None:
        assert file_collection != self._file_collection
        logger.debug("ThumbView.set_file_collection")
        self._file_collection = file_collection
        self._file_collection.sig_files_set.connect(self.on_file_collection_set)
        self._file_collection.sig_files_reordered.connect(self.on_file_collection_reordered)
        self._file_collection.sig_files_filtered.connect(self.on_file_collection_filtered)
        self._file_collection.sig_files_grouped.connect(self.on_file_collection_grouped)

        self._file_collection.sig_file_added.connect(self.on_file_added)
        self._file_collection.sig_file_removed.connect(self.on_file_removed)
        self._file_collection.sig_file_modified.connect(self.on_file_modified)
        self._file_collection.sig_fileinfo_updated.connect(self.on_fileinfo_updated)
        self._file_collection.sig_file_closed.connect(self.on_file_closed)

        self.on_file_collection_set()

    def on_file_added(self, fileinfo: FileInfo) -> None:
        logger.debug("ThumbView.on_file_added: %s", fileinfo)
        item = ThumbFileItem(fileinfo, self._controller, self)
        item._new = True
        self._location2item[fileinfo.location()].append(item)
        self._scene.addItem(item)
        self._items.append(item)

        self.style_item(item)

        if self._layout is not None:
            self._layout.append_item(item)
            self.refresh_bounding_rect()

    def on_file_removed(self, location: Location) -> None:
        logger.debug("ThumbView.on_file_removed: %s", location)
        items = self._location2item.get(location, [])
        for item in items:
            self._scene.removeItem(item)
            self._items.remove(item)

        if items != []:
            del self._location2item[location]
            self.layout_items()

    def on_file_modified(self, fileinfo: FileInfo) -> None:
        logger.debug("ThumbView.on_file_modified: %s", fileinfo)
        items = self._location2item.get(fileinfo.location(), [])
        for item in items:
            item.on_file_modified(fileinfo)
            item.update()

    def on_fileinfo_updated(self, fileinfo: FileInfo) -> None:
        logger.debug("ThumbView.on_fileinfo_updated: %s", fileinfo)
        items = self._location2item.get(fileinfo.location(), [])
        for item in items:
            item.on_fileinfo_updated(fileinfo)
            item.update()

    def on_file_closed(self, fileinfo: FileInfo) -> None:
        logger.debug("ThumbView.on_file_closed: %s", fileinfo)
        items = self._location2item.get(fileinfo.location(), [])
        for item in items:
            item.on_file_modified(fileinfo, final=True)
            item.update()

    def on_file_collection_reordered(self) -> None:
        logger.debug("ThumbView.on_file_collection_reordered")

        fileinfos = self._file_collection.get_fileinfos()

        # FIXME: this is a crude hack to deal with duplicate
        # Locations, this problem should probably be attacked in
        # FileCollection and asign a unique id to each FileInfo.
        self._items = []
        processed: Set[Location] = set()
        for fi in fileinfos:
            if fi.location() not in processed:
                lst = self._location2item[fi.location()]
                processed.add(fi.location())
                self._items += lst

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
        self._items.clear()
        self._cursor_item = None
        self._location2item.clear()
        self._scene.clear()
        self._layout = None

    def on_file_collection_set(self) -> None:
        logger.debug("ThumbView.on_file_collection_set")
        self.clear()

        fileinfos = self._file_collection.get_fileinfos()

        for fileinfo in fileinfos:
            item = ThumbFileItem(fileinfo, self._controller, self)
            self._location2item[fileinfo.location()].append(item)
            self._scene.addItem(item)
            self._items.append(item)

        self.style_items()
        self.layout_items()

    def resizeEvent(self, ev) -> None:
        logger.debug("ThumbView.resizeEvent: %s", ev)
        super().resizeEvent(ev)

        self.apply_zoom()

        if settings.value("globals/resize_delay", True):
            if self._resize_timer is not None:
                self.killTimer(self._resize_timer)
            self._resize_timer = self.startTimer(100)
        else:
            self.layout_items()

    def timerEvent(self, ev) -> None:
        if ev.timerId() == self._resize_timer:
            self.killTimer(self._resize_timer)
            self._resize_timer = None

            if self._layout is not None:
                self._layout.layout(self.viewport().width(), self.viewport().height())
            self.layout_items()
        else:
            assert False, "timer foobar"

    def style_item(self, item) -> None:
        if self._show_filtered:
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
        for item in self._items:
            self.style_item(item)

    def initPainter(self, painter):
        # logger.debug("ThumbView.initPainter:")
        pass

    @profile
    def paintEvent(self, ev) -> None:
        if self._needs_layout:
            self._layout_items()
            self._needs_layout = False

        super().paintEvent(ev)

    def layout_items(self):
        self._needs_layout = True
        self.invalidateScene()
        self.update()

    @profile
    def _layout_items(self) -> None:
        logger.debug("ThumbView._layout_items")

        self.setUpdatesEnabled(False)
        # old_item_index_method = self._scene.itemIndexMethod()
        # self._scene.setItemIndexMethod(QGraphicsScene.NoIndex)
        layout_builder = LayoutBuilder(self._scene, self._mode._tile_style)
        self._layout = layout_builder.build_layout(self._items)

        self._layout.layout(self.viewport().width(), self.viewport().height())
        self.refresh_bounding_rect()

        # self._scene.setItemIndexMethod(old_item_index_method)
        self.setUpdatesEnabled(True)

        logger.debug("ThumbView.layout_items: done")

    def refresh_bounding_rect(self) -> None:
        if self._layout is None:
            return

        def get_bounding_rect() -> QRectF:
            rect = self._layout.get_bounding_rect()

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
        self._mode.zoom_in()
        self.apply_zoom()

    def zoom_out(self) -> None:
        self._mode.zoom_out()
        self.apply_zoom()

    def less_details(self):
        self._mode.less_details()
        self.apply_zoom()

    def more_details(self):
        self._mode.more_details()
        self.apply_zoom()

    def set_style(self, item_style: FileItemStyle) -> None:
        self._mode = self._modes[item_style.value]
        self.apply_zoom()

    def apply_zoom(self) -> None:
        self._mode.update()

        if self._mode._zoom_index < 2:
            self.flavor = "normal"
        else:
            self.flavor = "large"

        for item in self._items:
            item.set_tile_size(self._mode._tile_style.tile_width, self._mode._tile_style.tile_height)

        self.style_items()
        self.layout_items()

        for item in self._items:
            item.update()

    def icon_from_fileinfo(self, fileinfo: FileInfo) -> QIcon:
        mimetype = self._controller.app.mime_database.get_mime_type(fileinfo.location())
        return self._controller.app.mime_database.get_icon_from_mime_type(mimetype)

    def receive_thumbnail(self, location: Location, flavor: str,
                          image: QImage, error_code: int, message: str) -> None:
        # receiving thumbnail for item that no longer exists is normal
        # when switching directories quickly
        items = self._location2item.get(location, [])
        for item in items:
            self.receive_thumbnail_for_item(item, flavor, image, error_code, message)
            item.set_thumbnail_image(image, flavor)

    def receive_thumbnail_for_item(self, item, flavor: str, image: QImage, error_code: int, message: str) -> None:
        if image is not None:
            item.set_thumbnail_image(image, flavor)
        else:
            if error_code is None:
                # thumbnail was generated, but couldn't be loaded
                item.set_thumbnail_image(None, flavor)
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

    def request_thumbnail(self, item, fileinfo: FileInfo, flavor: str, force: bool):
        self._controller.request_thumbnail(fileinfo, flavor, force)

    def reload_thumbnails(self):
        for item in self._items:
            item.reload_thumbnail()

    def set_show_filtered(self, show_filtered):
        self._show_filtered = show_filtered
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
        self._scene.clearSelection()

        if self._cursor_item is not None:
            self._cursor_item.update()
        self._cursor_item = self._location2item.get(fileinfo.location(), [None])[0]
        if self._cursor_item is not None:
            self._cursor_item.setSelected(True)
            self._cursor_item.update()

    def mousePressEvent(self, ev):
        super().mousePressEvent(ev)

        item = self._cursor_item
        if item is not None:
            self._cursor_item = None
            item.update()

    def contextMenuEvent(self, ev):
        if ev.reason() == QContextMenuEvent.Keyboard:
            if self._cursor_item is None:
                self._controller.on_context_menu(ev.globalPos())
            else:
                self._controller.on_item_context_menu(ev, self._cursor_item)
        else:
            super().contextMenuEvent(ev)
            if not ev.isAccepted():
                self._controller.on_context_menu(ev.globalPos())


# EOF #
