# dirtoo - File and directory manipulation tools for Python
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


from typing import TYPE_CHECKING, cast, Sequence, Dict, Optional, Set

import logging

import itertools
from collections import defaultdict

from PyQt6.QtCore import Qt, QRectF, QTimerEvent, QKeyCombination
from PyQt6.QtGui import (QBrush, QIcon, QColor, QPainter, QImage,
                         QKeySequence, QContextMenuEvent, QPaintEvent,
                         QMouseEvent, QMoveEvent, QKeyEvent, QResizeEvent, QShortcut)
from PyQt6.QtWidgets import QGraphicsView

from dirtoo.dbus_thumbnailer import DBusThumbnailerError
from dirtoo.filecollection.file_collection import FileCollection
from dirtoo.fileview.file_graphics_scene import FileGraphicsScene
from dirtoo.filesystem.file_info import FileInfo
from dirtoo.fileview.file_item import FileItem
from dirtoo.fileview.file_view_style import FileViewStyle
from dirtoo.fileview.layout import RootLayout
from dirtoo.fileview.layout_builder import LayoutBuilder
from dirtoo.gui.leap_widget import LeapWidget
from dirtoo.fileview.mode import Mode, IconMode, SequenceMode, DetailMode, FileItemStyle
from dirtoo.fileview.settings import settings
from dirtoo.filesystem.location import Location

if TYPE_CHECKING:
    from dirtoo.fileview.controller import Controller

logger = logging.getLogger(__name__)


class FileView(QGraphicsView):

    def __init__(self, controller: 'Controller') -> None:
        super().__init__()

        self._controller = controller

        self.setCacheMode(QGraphicsView.CacheModeFlag.CacheBackground)

        self.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        # self.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOn)

        self._show_filtered = False

        self._location2item: Dict[Location, list[FileItem]] = defaultdict(list)
        self.setAcceptDrops(True)

        self._scene = FileGraphicsScene()
        self._scene.sig_files_drop.connect(self._controller.on_files_drop)
        self.setScene(self._scene)

        self._scene.selectionChanged.connect(self.on_selection_changed)

        self._style = FileViewStyle()

        self._modes: Sequence[Mode] = [
            IconMode(self), SequenceMode(self),
            DetailMode(self)
        ]
        self._mode = self._modes[FileItemStyle.ICON.value]

        self._layout: Optional[RootLayout] = None

        self._items: list[FileItem] = []

        self._file_collection: Optional[FileCollection] = None

        self._needs_layout = True

        self.apply_zoom()
        self._cursor_item: Optional[FileItem] = None
        self._crop_thumbnails = False
        self.setBackgroundBrush(QBrush(Qt.GlobalColor.white, Qt.BrushStyle.SolidPattern))
        self._resize_timer: Optional[int] = None

        self.setDragMode(QGraphicsView.DragMode.RubberBandDrag)

        self.setRenderHints(QPainter.RenderHint.SmoothPixmapTransform |
                            QPainter.RenderHint.TextAntialiasing |
                            QPainter.RenderHint.Antialiasing)

        shortcut = QShortcut(QKeySequence(QKeyCombination(Qt.Modifier.CTRL, Qt.Key.Key_G)), self)
        shortcut.setContext(Qt.ShortcutContext.WidgetShortcut)
        shortcut.activated.connect(self._on_reset)

        shortcut = QShortcut(QKeySequence(Qt.Key.Key_Slash), self)
        shortcut.setContext(Qt.ShortcutContext.WidgetShortcut)
        shortcut.activated.connect(lambda: self._controller.show_location_toolbar(False))

        self._leap_widget = LeapWidget(self)
        self._leap_widget.sig_leap.connect(self.leap_to)

        self._scroll_timer: Optional[int] = None
        self._is_scrolling: bool = False
        self.verticalScrollBar().sliderReleased.connect(self._on_vertical_scrollbar_slider_released)
        self.verticalScrollBar().valueChanged.connect(self._on_vertical_scrollbar_slider_value_changed)

    def __del__(self) -> None:
        logger.debug("FileView.__del__")

    def _on_vertical_scrollbar_slider_released(self) -> None:
        self._is_scrolling = False

        if self._scroll_timer is not None:
            self.killTimer(self._scroll_timer)
            self._scroll_timer = None

        self._prepare_viewport()

    def _prepare_viewport(self) -> None:
        # print(self.viewport().rect())
        visible_items = self._scene.items(self.mapToScene(self.viewport().rect()))
        for item in visible_items:
            if isinstance(item, FileItem):
                item.prepare()

    def _on_vertical_scrollbar_slider_value_changed(self, value: int) -> None:
        self._is_scrolling = True

        if self._scroll_timer is not None:
            self.killTimer(self._scroll_timer)
            self._scroll_timer = None

        self._scroll_timer = self.startTimer(500)

    def is_scrolling(self) -> bool:
        return self._is_scrolling

    def _on_reset(self) -> None:
        self._controller.hide_all()

    def prepare(self) -> None:
        for item in self._items:
            item.prepare()

    def on_selection_changed(self) -> None:
        self._controller._update_info()

    def cursor_move(self, dx: int, dy: int) -> None:
        def best_item(items: Sequence[FileItem], rect: QRectF) -> FileItem:
            """Select the most top/left and fully visible item"""
            def contains(item: FileItem) -> bool:
                r = QRectF(item.tile_rect)
                r.moveTo(item.pos())
                return cast(bool, rect.contains(r))

            items = sorted(items, key=lambda item: (not contains(item),
                                                    item.pos().x(),
                                                    item.pos().y()))
            return items[0]

        if self._cursor_item is None:
            rect = self.mapToScene(self.rect()).boundingRect()
            items: Sequence[FileItem] = [item for item in self._scene.items(rect) if isinstance(item, FileItem)]
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
        items = [item for item in self._scene.items(rect) if isinstance(item, FileItem)]
        if items:
            self._cursor_item = items[0]

        self._cursor_item.update()
        self.ensureVisible(self._cursor_item)

    def keyPressEvent(self, ev: QKeyEvent) -> None:
        if ev.key() == Qt.Key.Key_Escape:
            self._scene.clearSelection()
            item = self._cursor_item
            self._cursor_item = None
            if item is not None:
                item.update()
        elif ev.key() == Qt.Key.Key_Space and ev.modifiers() & Qt.KeyboardModifier.ControlModifier:
            if self._cursor_item is not None:
                self._cursor_item.setSelected(not self._cursor_item.isSelected())
        elif ev.key() == Qt.Key.Key_Left:
            if self._cursor_item is not None and ev.modifiers() & Qt.KeyboardModifier.ShiftModifier:
                self._cursor_item.setSelected(True)
            self.cursor_move(-1, 0)
        elif ev.key() == Qt.Key.Key_Right:
            if self._cursor_item is not None and ev.modifiers() & Qt.KeyboardModifier.ShiftModifier:
                self._cursor_item.setSelected(True)
            self.cursor_move(+1, 0)
        elif ev.key() == Qt.Key.Key_Up:
            if self._cursor_item is not None and ev.modifiers() & Qt.KeyboardModifier.ShiftModifier:
                self._cursor_item.setSelected(True)
            self.cursor_move(0, -1)
        elif ev.key() == Qt.Key.Key_Down:
            if self._cursor_item is not None and ev.modifiers() & Qt.KeyboardModifier.ShiftModifier:
                self._cursor_item.setSelected(True)
            self.cursor_move(0, +1)
        elif ev.key() == Qt.Key.Key_Return:
            if self._cursor_item is not None:
                self._cursor_item.click_action(new_window=bool(ev.modifiers() & Qt.KeyboardModifier.ShiftModifier))
        elif ev.text() != "":
            self._leap_widget.show()
            self._leap_widget._line_edit.setText(ev.text())
            self._leap_widget._line_edit.setFocus()
        else:
            super().keyPressEvent(ev)

    def set_crop_thumbnails(self, v: bool) -> None:
        self._crop_thumbnails = v
        for item in self._items:
            item.update()

    def set_file_collection(self, file_collection: FileCollection) -> None:
        assert file_collection != self._file_collection
        logger.debug("FileView.set_file_collection")
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

        self._prepare_viewport()

    def on_file_added(self, idx: int, fileinfo: FileInfo) -> None:
        logger.debug("FileView.on_file_added: %s %s", idx, fileinfo)
        item = FileItem(fileinfo, self._controller, self)
        item._new = True
        self._location2item[fileinfo.location()].append(item)
        self._scene.addItem(item)
        self._items.append(item)

        self.style_item(item)

        if self._layout is not None:
            self._layout.append_item(item)
            self.refresh_bounding_rect()

    def on_file_removed(self, location: Location) -> None:
        logger.debug("FileView.on_file_removed: %s", location)
        items = self._location2item.get(location, [])
        for item in items:
            self._scene.removeItem(item)
            self._items.remove(item)

        if items != []:
            del self._location2item[location]
            self.layout_items()

    def on_file_modified(self, fileinfo: FileInfo) -> None:
        logger.debug("FileView.on_file_modified: %s", fileinfo)
        items = self._location2item.get(fileinfo.location(), [])
        for item in items:
            item.on_file_modified(fileinfo)
            item.update()

    def on_fileinfo_updated(self, fileinfo: FileInfo) -> None:
        logger.debug("FileView.on_fileinfo_updated: %s", fileinfo)
        items = self._location2item.get(fileinfo.location(), [])
        for item in items:
            item.on_fileinfo_updated(fileinfo)
            item.update()

    def on_file_closed(self, fileinfo: FileInfo) -> None:
        logger.debug("FileView.on_file_closed: %s", fileinfo)
        items = self._location2item.get(fileinfo.location(), [])
        for item in items:
            item.on_file_modified(fileinfo, final=True)
            item.update()

    def on_file_collection_reordered(self) -> None:
        logger.debug("FileView.on_file_collection_reordered")
        assert self._file_collection

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
        logger.debug("FileView.on_file_collection_filtered")
        self.style_items()
        self.layout_items()

    def on_file_collection_grouped(self) -> None:
        logger.debug("FileView.on_file_collection_grouped")
        self.style_items()
        self.layout_items()

    def clear(self) -> None:
        self._items.clear()
        self._cursor_item = None
        self._location2item.clear()
        self._scene.clear()
        self._layout = None

    def on_file_collection_set(self) -> None:
        logger.debug("FileView.on_file_collection_set")
        assert self._file_collection is not None

        self.clear()
        fileinfos = self._file_collection.get_fileinfos()

        for fileinfo in fileinfos:
            item = FileItem(fileinfo, self._controller, self)
            self._location2item[fileinfo.location()].append(item)
            self._scene.addItem(item)
            self._items.append(item)

        self.style_items()
        self.layout_items()

    def resizeEvent(self, ev: QResizeEvent) -> None:
        logger.debug("FileView.resizeEvent: %s", ev)

        super().resizeEvent(ev)

        self.apply_zoom()

        if settings.value("globals/resize_delay", True):
            if self._resize_timer is not None:
                self.killTimer(self._resize_timer)
            self._resize_timer = self.startTimer(100)
        else:
            self.layout_items()

        self._leap_widget.place_widget()

    def moveEvent(self, ev: QMoveEvent) -> None:
        super().moveEvent(ev)
        self._leap_widget.place_widget()

    def timerEvent(self, ev: QTimerEvent) -> None:
        if ev.timerId() == self._resize_timer:
            assert self._resize_timer is not None
            self.killTimer(self._resize_timer)
            self._resize_timer = None

            if self._layout is not None:
                self._layout.layout(self.viewport().width(), self.viewport().height())
            self.layout_items()
        elif ev.timerId() == self._scroll_timer:
            assert self._scroll_timer is not None
            self.killTimer(self._scroll_timer)
            self._is_scrolling = False
            self._scroll_timer = None
            self._prepare_viewport()
        else:
            assert False, "timer foobar: {}".format(ev.timerId())

    def style_item(self, item: FileItem) -> None:
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

    def initPainter(self, painter: QPainter) -> None:
        # logger.debug("FileView.initPainter:")
        pass

    def paintEvent(self, ev: QPaintEvent) -> None:
        if self._needs_layout:
            self._layout_items()
            self._needs_layout = False

        super().paintEvent(ev)

    def layout_items(self) -> None:
        self._needs_layout = True
        self.invalidateScene()
        self.update()

    def _layout_items(self) -> None:
        logger.debug("FileView._layout_items")

        self.setUpdatesEnabled(False)
        # old_item_index_method = self._scene.itemIndexMethod()
        # self._scene.setItemIndexMethod(QGraphicsScene.NoIndex)
        layout_builder = LayoutBuilder(self._scene, self._mode._tile_style)
        self._layout = layout_builder.build_layout(self._items)

        self._layout.layout(self.viewport().width(), self.viewport().height())
        self.refresh_bounding_rect()

        # self._scene.setItemIndexMethod(old_item_index_method)
        self.setUpdatesEnabled(True)

        logger.debug("FileView.layout_items: done")

    def refresh_bounding_rect(self) -> None:
        if self._layout is None:
            return

        def get_bounding_rect() -> QRectF:
            assert self._layout is not None

            rect = self._layout.get_bounding_rect()

            if True:  # self.layout_style == LayoutStyle.ROWS:
                # center alignment
                w = rect.right()
            else:
                # top/center alignment
                w = max(self.viewport().width(), rect.right())  # type: ignore
            h = max(self.viewport().height(), rect.bottom())

            return QRectF(0, 0, w, h)

        self.setSceneRect(get_bounding_rect())

    def zoom_in(self) -> None:
        self._mode.zoom_in()
        self.apply_zoom()

    def zoom_out(self) -> None:
        self._mode.zoom_out()
        self.apply_zoom()

    def less_details(self) -> None:
        self._mode.less_details()
        self.apply_zoom()

    def more_details(self) -> None:
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

    def receive_thumbnail(self, location: Location,
                          flavor: Optional[str], image: Optional[QImage],
                          error_code: Optional[DBusThumbnailerError], message: Optional[str]) -> None:
        # receiving thumbnail for item that no longer exists is normal
        # when switching directories quickly
        items = self._location2item.get(location, [])
        for item in items:
            self.receive_thumbnail_for_item(item, flavor, image, error_code, message)
            item.set_thumbnail_image(image, flavor)

    def receive_thumbnail_for_item(self, item: FileItem,
                                   flavor: Optional[str], image: Optional[QImage],
                                   error_code: Optional[DBusThumbnailerError], message: Optional[str]) -> None:
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

    def request_thumbnail(self, item: FileItem, fileinfo: FileInfo, flavor: str, force: bool) -> None:
        self._controller.request_thumbnail(fileinfo, flavor, force)

    def reload_thumbnails(self) -> None:
        for item in self._items:
            item.reload_thumbnail()

    def set_show_filtered(self, show_filtered: bool) -> None:
        self._show_filtered = show_filtered
        self.style_items()
        self.layout_items()

    def set_filtered(self, filtered: bool) -> None:
        if filtered:
            self.setBackgroundBrush(QBrush(QColor(220, 220, 255), Qt.BrushStyle.SolidPattern))
        else:
            self.setBackgroundBrush(QBrush())

    def scroll_by(self, x: int, y: int) -> None:
        scrollbar = self.verticalScrollBar()
        scrollbar.setValue(scrollbar.value() + y)

        scrollbar = self.horizontalScrollBar()
        scrollbar.setValue(scrollbar.value() + x)

    def set_cursor_to_fileinfo(self, fileinfo: 'FileInfo', ensure_visible: bool) -> None:
        self._scene.clearSelection()

        if self._cursor_item is not None:
            self._cursor_item.update()

        if fileinfo.location() in self._location2item:
            self._cursor_item = self._location2item[fileinfo.location()][0]
            self._cursor_item.setSelected(True)
            self._cursor_item.update()
            if ensure_visible:
                self.ensureVisible(self._cursor_item)

    def mousePressEvent(self, ev: QMouseEvent) -> None:
        super().mousePressEvent(ev)

        item = self._cursor_item
        if item is not None:
            self._cursor_item = None
            item.update()

    def contextMenuEvent(self, ev: QContextMenuEvent) -> None:
        if ev.reason() == QContextMenuEvent.Reason.Keyboard:
            if self._cursor_item is None:
                self._controller.on_context_menu(ev.globalPos())
            else:
                self._controller.show_item_context_menu(self._cursor_item, None)
        else:
            super().contextMenuEvent(ev)
            if not ev.isAccepted():
                self._controller.on_context_menu(ev.globalPos())

    def leap_to(self, text: str, forward: bool, skip: bool) -> None:
        assert self._file_collection is not None

        if text == "":
            item = self._cursor_item
            self._cursor_item = None
            if item is not None:
                item.update()
        else:
            text = text.lower()

            item = self._cursor_item
            if item is not None:
                try:
                    idx = self._file_collection.index(item.fileinfo)
                except ValueError:
                    idx = None
            else:
                idx = None

            if forward:
                if idx is None:
                    idx = 0
                elif skip:
                    idx += 1

                for fi in itertools.chain(self._file_collection[idx:],
                                          self._file_collection[0:idx]):
                    if fi.basename().lower().startswith(text):
                        self.set_cursor_to_fileinfo(fi, True)
                        break
            else:
                if idx is None:
                    idx = len(self._file_collection)
                elif skip:
                    idx -= 1

                for fi in itertools.chain(reversed(self._file_collection[0:idx + 1]),
                                          reversed(self._file_collection[idx:])):
                    if fi.basename().lower().startswith(text):
                        self.set_cursor_to_fileinfo(fi, True)
                        break

    def scroll_top(self) -> None:
        self.ensureVisible(0, 0, 1, 1)

    def scroll_bottom(self) -> None:
        assert self._layout is not None
        self.ensureVisible(0, self._layout.get_bounding_rect().height(), 1, 1)


# EOF #
