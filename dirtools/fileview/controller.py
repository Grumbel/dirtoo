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

import io
import logging
import os

from PyQt5.QtCore import QObject, Qt, pyqtSignal, QUrl, QMimeData
from PyQt5.QtGui import QClipboard

import bytefmt

from dirtools.fileview.file_collection import FileCollection
from dirtools.fileview.grouper import (Grouper, DayGrouperFunc,
                                       DirectoryGrouperFunc,
                                       NoGrouperFunc,
                                       DurationGrouperFunc)
from dirtools.fileview.directory_watcher import DirectoryWatcher
from dirtools.fileview.filter_parser import FilterParser
from dirtools.fileview.settings import settings
from dirtools.fileview.filelist_stream import FileListStream
from dirtools.fileview.location import Location, Payload
from dirtools.fileview.file_info import FileInfo
from dirtools.fileview.search_stream import SearchStream
from dirtools.fileview.thumb_view import FileItemStyle
from dirtools.fileview.gnome import parse_gnome_copied_files, make_gnome_copied_files
from dirtools.util import make_non_existing_filename
from dirtools.fileview.path_completion import PathCompletion

if False:
    from dirtools.fileview.application import FileViewApplication  # noqa: F401

logger = logging.getLogger(__name__)


class Controller(QObject):

    sig_location_changed = pyqtSignal(Location)
    sig_location_changed_to_none = pyqtSignal()

    def __init__(self, app: 'FileViewApplication') -> None:
        from dirtools.fileview.gui import Gui

        super().__init__()

        from dirtools.fileview.filter import Filter
        from dirtools.fileview.sorter import Sorter
        from dirtools.fileview.actions import Actions

        self.app: 'FileViewApplication' = app
        self.location: Optional[Location] = None
        self.file_collection = FileCollection()
        self.actions = Actions(self)
        self._gui = Gui(self)

        self._filter = Filter()
        self._sorter = Sorter(self)
        self._grouper = Grouper()

        self._location_history: List[Location] = []
        self._location_history_index = 0

        self._directory_watcher: Optional[DirectoryWatcher] = None
        self._filelist_stream: Optional[FileListStream] = None
        self._search_stream: Optional[SearchStream] = None

        self._gui._window.thumb_view.set_file_collection(self.file_collection)

        self.app.metadata_collector.sig_metadata_ready.connect(self.receive_metadata)

        self._path_completion = PathCompletion()
        self._path_completion.sig_completions_ready.connect(self._on_completions)
        self._path_completion.start()
        self._apply_settings()

    def close_streams(self) -> None:
        self._gui._window._message_area.hide()

        if self._directory_watcher is not None:
            self._directory_watcher.close()
            self._directory_watcher = None

        if self._filelist_stream is not None:
            self._filelist_stream.close()
            self._filelist_stream = None

        if self._search_stream is not None:
            self._search_stream.close()
            self._search_stream = None

    def close(self) -> None:
        self.close_streams()
        self._path_completion.close()

    def _apply_settings(self) -> None:
        v = settings.value("globals/crop_thumbnails", False, bool)
        self.actions.crop_thumbnails.setChecked(v)
        self.actions.crop_thumbnails.triggered.emit()

    def save_as(self) -> None:
        filename = self._gui.get_savefilename()
        if filename is not None:
            self.file_collection.save_as(filename)

    def on_exit(self) -> None:
        self.app.close_controller(self)

    def show_hidden(self) -> None:
        self._filter.show_hidden = not self._filter.show_hidden
        settings.set_value("globals/show_hidden", self._filter.show_hidden)
        self.apply_filter()

    def show_filtered(self) -> None:
        self._gui._window.thumb_view.set_show_filtered(not self._gui._window.thumb_view.show_filtered)

    def show_abspath(self) -> None:
        pass

    def show_basename(self) -> None:
        pass

    def view_detail_view(self) -> None:
        self._gui._window.thumb_view.set_style(FileItemStyle.DETAIL)

    def view_icon_view(self) -> None:
        self._gui._window.thumb_view.set_style(FileItemStyle.ICON)

    def view_small_icon_view(self) -> None:
        self._gui._window.thumb_view.set_style(FileItemStyle.SMALLICON)

    def zoom_in(self) -> None:
        self._gui._window.thumb_view.zoom_in()

    def zoom_out(self) -> None:
        self._gui._window.thumb_view.zoom_out()

    def less_details(self) -> None:
        self._gui._window.thumb_view.less_details()

    def show_filter_help(self) -> None:
        parser = FilterParser(self._filter)

        fout = io.StringIO()
        parser.print_help(fout)

        self._gui.show_help(fout.getvalue())

    def show_search_help(self) -> None:
        parser = FilterParser(self._filter)

        fout = io.StringIO()
        parser.print_help(fout)

        self._gui.show_help(fout.getvalue())

    def more_details(self) -> None:
        self._gui._window.thumb_view.more_details()

    def set_filter(self, pattern):
        parser = FilterParser(self._filter)
        parser.parse(pattern)
        self.apply_filter()

    def go_forward(self) -> None:
        if self._location_history != []:
            self._location_history_index = min(self._location_history_index + 1, len(self._location_history) - 1)
            self._location_history[self._location_history_index]
            self.set_location(self._location_history[self._location_history_index], track_history=False)
            if self._location_history_index == len(self._location_history) - 1:
                self.actions.forward.setEnabled(False)
            self.actions.back.setEnabled(True)

    def go_back(self) -> None:
        if self._location_history != []:
            self._location_history_index = max(self._location_history_index - 1, 0)
            self.set_location(self._location_history[self._location_history_index], track_history=False)
            if self._location_history_index == 0:
                self.actions.back.setEnabled(False)
            self.actions.forward.setEnabled(True)

    def go_home(self) -> None:
        home = os.path.expanduser("~")
        self.set_location(Location.from_path(home))

    def _on_archive_extractor_finished(self) -> None:
        logger.info("Controller._on_archive_extractor_finished")
        self._gui._window.hide_loading()

    def _on_finished(self) -> None:
        logger.info("Controller._on_finished")
        self._gui._window.hide_loading()

        self.apply_sort()
        self.apply_filter()
        self.apply_grouper()

    def _on_scandir_finished(self, fileinfos) -> None:
        logger.info("Controller._on_scandir_extractor_finished")
        self._gui._window.hide_loading()

        self.file_collection.set_fileinfos(fileinfos)
        self.apply_sort()
        self.apply_filter()
        self.apply_grouper()

    def _on_directory_watcher_message(self, message):
        self._gui._window._message_area.show_error(message)

    def set_location(self, location: Location, track_history=True) -> None:
        self.close_streams()
        self._gui._window.search_toolbar.hide()
        if not self.actions.filter_pin.isChecked():
            self.set_filter("")
            self._gui._window.filter_toolbar.hide()
        self._gui._window.thumb_view.setFocus()

        self.app.location_history.append(location)

        if track_history:
            self._location_history = self._location_history[0:self._location_history_index + 1]
            self._location_history_index = len(self._location_history)
            self._location_history.append(location)
            self.actions.back.setEnabled(True)
            self.actions.forward.setEnabled(False)

        self._gui._window.show_loading()

        self._set_directory_location(location)

        self.sig_location_changed.emit(location)

    def _set_directory_location(self, location: Location) -> None:
        self.file_collection.clear()

        if self._directory_watcher is not None:
            self._directory_watcher.close()
        self._directory_watcher = self.app.vfs.opendir(location)

        if hasattr(self._directory_watcher, 'sig_file_added'):
            self._directory_watcher.sig_file_added.connect(self.file_collection.add_fileinfo)

        if hasattr(self._directory_watcher, 'sig_file_removed'):
            self._directory_watcher.sig_file_removed.connect(self.file_collection.remove_file)

        if hasattr(self._directory_watcher, 'sig_file_modified'):
            self._directory_watcher.sig_file_modified.connect(self.file_collection.modify_file)

        if hasattr(self._directory_watcher, 'sig_file_closed'):
            self._directory_watcher.sig_file_closed.connect(self.file_collection.close_file)

        if hasattr(self._directory_watcher, 'sig_finished'):
            self._directory_watcher.sig_finished.connect(self._on_finished)

        if hasattr(self._directory_watcher, 'sig_scandir_finished'):
            self._directory_watcher.sig_scandir_finished.connect(self._on_scandir_finished)

        if hasattr(self._directory_watcher, 'sig_message'):
            self._directory_watcher.sig_message.connect(self._on_directory_watcher_message)

        self._directory_watcher.start()

        if location.protocol() == "search":
            abspath, query = location.search_query()
            search_location = Location.from_path(abspath)
            self.location = search_location
            self._gui._window.set_location(search_location)
            self._gui._window.search_lineedit.setText(query)
            self._gui._window.search_toolbar.show()
        else:
            self.location = location
            self._gui._window.set_location(self.location)

    def set_files(self, files: List[Location]) -> None:
        self._gui._window.set_file_list()

        self.location = None
        self.file_collection.set_fileinfos([self.app.vfs.get_fileinfo(f) for f in files])
        self.apply_sort()
        self.apply_filter()
        self.apply_grouper()

        self.sig_location_changed_to_none.emit()

    def new_controller(self, clone: bool=False) -> 'Controller':
        controller = self.app.new_controller()
        if clone and self.location is not None:
            controller.set_location(self.location)
        return controller

    def show_file_history(self) -> None:
        self.set_location(Location.from_url("history:///"))

    def apply_grouper(self) -> None:
        logger.debug("Controller.apply_grouper")
        self.file_collection.group(self._grouper)

    def apply_sort(self) -> None:
        logger.debug("Controller.apply_sort")
        self._sorter.apply(self.file_collection)

    def apply_filter(self) -> None:
        logger.debug("Controller.apply_filter")
        self.file_collection.filter(self._filter)
        self._update_info()

    def _update_info(self) -> None:
        fileinfos = self.file_collection.get_fileinfos()

        filtered_count = 0
        hidden_count = 0
        total_size = 0
        visible_total_size = 0
        for fileinfo in fileinfos:
            if fileinfo.is_hidden:
                hidden_count += 1
            elif fileinfo.is_excluded:
                filtered_count += 1
            else:
                visible_total_size += fileinfo.size()
            total_size += fileinfo.size()

        total = self.file_collection.size()

        msg = "{} visible ({}), {} filtered, {} hidden, {} total ({})".format(
            total - filtered_count - hidden_count,
            bytefmt.humanize(visible_total_size),
            filtered_count,
            hidden_count,
            total,
            bytefmt.humanize(total_size))

        selected_items = self._gui._window.thumb_view._scene.selectedItems()
        if selected_items != []:
            total_size = 0
            for item in selected_items:
                total_size += item.fileinfo.size()
            msg += ", {} selected ({})".format(len(selected_items), bytefmt.humanize(total_size))

        self._gui._window.show_info(msg)

        self._gui._window.thumb_view.set_filtered(filtered_count > 0)

    def toggle_timegaps(self) -> None:
        pass

    def parent_directory(self, new_window: bool=False):
        if self.location is not None:
            if new_window:
                self.app.show_location(self.location.parent())
            else:
                self.set_location(self.location.parent())

    def on_click(self, fileinfo: FileInfo, new_window=False) -> None:
        self._gui._window.thumb_view.set_cursor_to_fileinfo(fileinfo)

        if not fileinfo.isdir():
            if fileinfo.is_archive() and settings.value("globals/open_archives", True, bool):
                location = fileinfo.location().copy()
                location._payloads.append(Payload("archive", ""))

                if new_window:
                    self.new_controller().set_location(location)
                else:
                    self.set_location(location)
            else:
                self.app.file_history.append(fileinfo.location())
                self.app.executor.open(fileinfo.location())
        else:
            if self.location is None or new_window:
                logger.info("Controller.on_click: app.show_location: %s", fileinfo)
                self.app.show_location(fileinfo.location())
            else:
                logger.info("Controller.on_click: self.set_location: %s", fileinfo)
                self.set_location(fileinfo.location())

    def clear_selection(self) -> None:
        self._gui._window.thumb_view._scene.clearSelection()

    def select_all(self) -> None:
        scene = self._gui._window.thumb_view._scene
        oldstate = scene.blockSignals(True)
        for item in scene.items():
            item.setSelected(True)
        scene.blockSignals(oldstate)
        scene.selectionChanged.emit()

    def on_context_menu(self, pos) -> None:
        self._gui.on_context_menu(pos)

    def on_item_context_menu(self, ev, item) -> None:
        self._gui.on_item_context_menu(ev, item)

    def show_current_filename(self, filename: str) -> None:
        self._gui._window.show_current_filename(filename)

    def add_files(self, files: List[Location]) -> None:
        for location in files:
            self.file_collection.add_fileinfo(self.app.vfs.get_fileinfo(location))

    def set_crop_thumbnails(self, v: bool) -> None:
        settings.set_value("globals/crop_thumbnails", v)
        self._gui._window.thumb_view.set_crop_thumbnails(v)

    def request_metadata(self, fileinfo: FileInfo) -> None:
        self.app.metadata_collector.request_metadata(fileinfo.location())

    def receive_metadata(self, location: Location, metadata: Dict[str, object]) -> None:
        logger.debug("Controller.receive_metadata: %s %s", location, metadata)
        fileinfo = self.file_collection.get_fileinfo(location)
        if fileinfo is None:
            logger.error("Controller.receive_metadata: not found fileinfo for %s", location)
        else:
            fileinfo.metadata().update(metadata)
            self.file_collection.update_fileinfo(fileinfo)

    def request_thumbnail(self, fileinfo: FileInfo, flavor: str, force: bool) -> None:
        self.app.thumbnailer.request_thumbnail(fileinfo.location(), flavor, force,
                                               self.receive_thumbnail)

    def prepare(self) -> None:
        self._gui._window.thumb_view.prepare()

    def reload(self) -> None:
        if self.location is not None:
            self.set_location(self.location)
        else:
            self._gui._window.set_file_list()

            fileinfos = self.file_collection.get_fileinfos()
            fileinfos = (self.app.vfs.get_fileinfo(f.location()) for f in fileinfos)
            self.file_collection.set_fileinfos(fileinfos)

            self.apply_sort()
            self.apply_filter()
            self.apply_grouper()

    def receive_thumbnail(self, location: Location, flavor: str,
                          pixmap, error_code: int, message: str) -> None:
        logger.debug("Controller.receive_thumbnail: %s %s %s %s %s",
                     location, flavor, pixmap, error_code, message)
        if pixmap is None:
            logger.error("Controller.receive_thumbnail: error: %s  %s  %s", location, error_code, message)

        self._gui._window.thumb_view.receive_thumbnail(location, flavor, pixmap, error_code, message)

    def reload_thumbnails(self) -> None:
        selected_items = self._gui._window.thumb_view._scene.selectedItems()
        files = [item.fileinfo.abspath()
                 for item in selected_items]
        self.app.dbus_thumbnail_cache.delete(files)

        for item in selected_items:
            item.reload_thumbnail()

    def set_grouper_by_none(self) -> None:
        self._grouper.set_func(NoGrouperFunc())
        self.apply_grouper()

    def set_grouper_by_directory(self) -> None:
        self._grouper.set_func(DirectoryGrouperFunc())
        self.apply_grouper()

    def set_grouper_by_day(self) -> None:
        self._grouper.set_func(DayGrouperFunc())
        self.apply_grouper()

    def set_grouper_by_duration(self) -> None:
        self._grouper.set_func(DurationGrouperFunc())
        self.apply_grouper()

    def show_rename_dialog(self, location: Optional[Location] = None) -> None:
        if location is None:
            item = self._gui._window.thumb_view._cursor_item
            if item is None:
                logger.error("no file selected for renaming")
                return

            location = item.fileinfo.location()

        self.app.fs_operations.rename_location(location, parent=self._gui._window)

    def toggle_bookmark(self) -> bool:
        """Returns true if a bookmark was set, false otherwise"""

        if self.location is not None:
            if self.has_bookmark():
                self.app.bookmarks.remove(self.location)
                return False
            else:
                self.app.bookmarks.append(self.location)
                return True
            True
        else:
            return False

    def has_bookmark(self) -> bool:
        entries = self.app.bookmarks.get_entries()
        return bool(self.location in entries)

    def show_search(self) -> None:
        self._gui._window.search_toolbar.show()
        self._gui._window.search_lineedit.setFocus()

    def hide_search_toolbar(self) -> None:
        self._gui._window.search_toolbar.hide()
        self._gui._window.thumb_view.setFocus()
        self.set_location(self.location)

    def show_location_toolbar(self, selectall=True) -> None:
        # self._gui._window.search_toolbar.hide()
        # self._gui._window.location_toolbar.show()
        self._gui._window.location_lineedit.setFocus(Qt.ShortcutFocusReason)
        if selectall is False:
            self._gui._window.location_lineedit.set_cursor_to_end()
            text = self._gui._window.location_lineedit.text()
            if text and text[-1] != "/":
                new_text = text + "/"
                self._gui._window.location_lineedit.setText(new_text)
                self._gui._window.location_lineedit.on_text_edited(new_text)
        else:
            self._gui._window.location_lineedit.selectAll()

    def hide_all(self):
        self._gui._window.search_toolbar.hide()
        self._gui._window.filter_toolbar.hide()

    def start_search(self, query):
        if self.location is None:
            abspath = "/tmp"
        else:
            abspath = self.app.vfs.get_stdio_name(self.location)

        location = Location.from_search_query(abspath, query)
        self.set_location(location)

        self._gui._window.thumb_view.setFocus()

    def close_search(self):
        self._gui._window.search_toolbar.hide()
        self._gui._window.thumb_view.setFocus()
        self._gui._window.location_lineedit.on_return_pressed()

    def close_window(self):
        self._gui._window.close()

    def set_filter_pin(self, value):
        # logic is handled in Controller.set_location()
        pass

    def show_preferences(self):
        self.app._preferences_dialog.show()
        self.app._preferences_dialog.raise_()

    def on_edit_cut(self):
        logger.debug("cut data to clipboard")

        mime_data = self.selection_to_mimedata(action=Qt.MoveAction)

        clipboard = self.app.qapp.clipboard()
        clipboard.setMimeData(mime_data, QClipboard.Clipboard)

    def on_edit_copy(self):
        logger.debug("copying data to clipboard")

        mime_data = self.selection_to_mimedata(action=Qt.CopyAction)

        clipboard = self.app.qapp.clipboard()
        clipboard.setMimeData(mime_data, QClipboard.Clipboard)

    def on_edit_paste(self):
        if self.location is not None:
            self.on_edit_paste_into(self.location)

    def on_edit_paste_into(self, location: Location):
        logger.debug("pasting data into folder from clipboard")

        if location is None:
            logger.error("trying to paste into None")
            return

        clipboard = self.app.qapp.clipboard()
        mime_data = clipboard.mimeData(QClipboard.Clipboard)

        if False:
            for fmt in mime_data.formats():
                print("Format:", fmt)
                print(mime_data.data(fmt))
                print()
            print()

        destination_path = location.get_path()

        if mime_data.hasFormat("x-special/gnome-copied-files"):
            try:
                action, urls = parse_gnome_copied_files(
                    mime_data.data("x-special/gnome-copied-files"))
            except Exception as err:
                logger.error("failed to parse clipboard data: %s", err)
            else:
                sources = [url.toLocalFile() for url in urls]
                self.app.fs.do_files(action, sources, destination_path)
        elif mime_data.hasUrls():
            urls = mime_data.urls()
            sources = [url.toLocalFile() for url in urls]
            self.app.fs.copy_files(sources, destination_path)
        else:
            logger.debug("unhandled format on paste")

        # https://www.uninformativ.de/blog/postings/2017-04-02/0/POSTING-en.html

    def selected_fileinfos(self):
        selected_items = self._gui._window.thumb_view._scene.selectedItems()
        return [item.fileinfo
                for item in selected_items]

    def selection_to_mimedata(self, uri_only=False, action: Qt.DropActions = None):
        mime_data = QMimeData()

        fileinfos = self.selected_fileinfos()

        urls = [QUrl.fromLocalFile(fi.abspath())
                for fi in fileinfos]
        mime_data.setUrls(urls)

        if not uri_only:
            text = "\n".join([fi.abspath()
                              for fi in fileinfos])
            mime_data.setText(text)

            if action is not None:
                gnome_copied_files = make_gnome_copied_files(action, urls)
                mime_data.setData("x-special/gnome-copied-files", gnome_copied_files)

        return mime_data

    def on_files_drop(self, action, urls, destination):
        if destination is None:
            destination = self.location

        if destination is None:
            return

        if destination.has_payload():
            logger.error("can't drop to a destination with payload: %s", destination)
            return

        sources = [url.toLocalFile() for url in urls]

        destination_path = destination.get_path()

        self.app.fs.do_files(action, sources, destination_path)

    def create_directory(self):
        if self.location is None:
            return

        if not self.location.has_stdio_name():
            logger.error("can't create directory in non-stdio locations")
            return

        suggested_name = make_non_existing_filename(self.location.get_stdio_name(),
                                                    "New Folder")

        name = self._gui.show_create_directory_dialog(suggested_name)
        if name is not None:
            abspath = os.path.join(self.location.get_stdio_name(), name)
            try:
                self.app.fs.create_directory(abspath)
            except Exception as err:
                self._gui.show_error("Error on file creation",
                                     "Error while trying to create directory:\n\n" + str(err))

    def create_file(self):
        if self.location is None or not self.location.has_stdio_name():
            return

        if not self.location.has_stdio_name():
            logger.error("can't create file in non-stdio locations")
            return

        suggested_name = make_non_existing_filename(self.location.get_stdio_name(),
                                                    "New File")

        name = self._gui.show_create_file_dialog(suggested_name)
        if name is not None:
            abspath = os.path.join(self.location.get_stdio_name(), name)
            try:
                self.app.fs.create_file(abspath)
            except Exception as err:
                self._gui.show_error("Error on file creation",
                                     "Error while trying to create file:\n\n" + str(err))

    def show_about_dialog(self) -> None:
        self._gui.show_about_dialog()

    def show_properties_dialog(self, fileinfo: FileInfo) -> None:
        self._gui.show_properties_dialog(fileinfo)

    def request_completions(self, text):
        self._path_completion.request_completions(text)

    def _on_completions(self, longest: str, candidates: List[str]):
        self._gui._window.location_lineedit.on_completions(longest, candidates)


# EOF #
