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

import re
import logging
import os
import subprocess

from PyQt5.QtWidgets import QFileDialog
from PyQt5.QtCore import QObject, QThread

from dirtools.fileview.actions import Actions
from dirtools.fileview.file_collection import FileCollection
from dirtools.fileview.file_view_window import FileViewWindow
from dirtools.util import expand_file
from dirtools.fileview.filter import Filter
from dirtools.fileview.sorter import Sorter
from dirtools.fileview.directory_watcher import DirectoryWatcher


VIDEO_EXT = ['wmv', 'mp4', 'mpg', 'mpeg', 'avi', 'flv', 'mkv', 'wmv',
             'mov', 'webm', 'f4v', 'flv', 'divx']

VIDEO_REGEX = r"\.({})$".format("|".join(VIDEO_EXT))


IMAGE_EXT = ['jpg', 'jpeg', 'gif', 'png', 'tif', 'tiff', 'webp']

IMAGE_REGEX = r"\.({})$".format("|".join(IMAGE_EXT))


ARCHIVE_EXT = ['zip', 'rar', 'tar', 'gz', 'xz', 'bz2', 'ar', '7z']

ARCHIVE_REGEX = r"\.({})$".format("|".join(ARCHIVE_EXT))


class Controller(QObject):

    def __init__(self, app):
        super().__init__()
        self.app = app
        self.location = None
        self.file_collection = FileCollection()
        self.actions = Actions(self)
        self.window = FileViewWindow(self)
        self.filter = Filter()
        self.sorter = Sorter(self)
        self.history: List[str] = []
        self.history_index = 0

        self.directory_watcher_thread = None
        self.directory_watcher = None

        self.window.file_view.set_file_collection(self.file_collection)
        self.window.thumb_view.set_file_collection(self.file_collection)

    def save_as(self):
        options = QFileDialog.Options()
        # options |= QFileDialog.DontUseNativeDialog
        filename, kind = QFileDialog.getSaveFileName(
            self.window,
            "QFileDialog.getSaveFileName()",
            "",  # dir
            "URL List (*.urls);;Path List (*.txt);;NUL Path List (*.nlst)",
            options=options)

        if filename != "":
            self.file_collection.save_as(filename)

    def close(self):
        self.window.close()

    def show_hidden(self):
        self.filter.show_hidden = not self.filter.show_hidden
        self.apply_filter()

    def show_filtered(self):
        self.window.thumb_view.set_show_filtered(not self.window.thumb_view.show_filtered)

    def show_abspath(self):
        self.window.file_view.show_abspath()

    def show_basename(self):
        self.window.file_view.show_basename()

    def view_detail_view(self):
        self.window.show_file_view()

    def show_thumb_view(self):
        self.window.show_thumb_view()

    def view_icon_view(self):
        pass

    def view_small_icon_view(self):
        pass

    def zoom_in(self):
        self.window.thumb_view.zoom_in()

    def zoom_out(self):
        self.window.thumb_view.zoom_out()

    def less_details(self):
        self.window.thumb_view.less_details()

    def more_details(self):
        self.window.thumb_view.more_details()

    def set_filter(self, pattern):
        if pattern == "":
            self.filter.set_pattern(None)
        elif pattern.startswith("/"):
            command, *arg = pattern[1:].split("/", 1)
            if command in ["video", "videos", "vid", "vids"]:
                self.filter.set_regex_pattern(VIDEO_REGEX, re.IGNORECASE)
            elif command in ["image", "images", "img", "imgs"]:
                self.filter.set_regex_pattern(IMAGE_REGEX, re.IGNORECASE)
            elif command in ["archive", "archives", "arch", "ar"]:
                self.filter.set_regex_pattern(ARCHIVE_REGEX, re.IGNORECASE)
            elif command in ["r", "rx", "re", "regex"]:
                self.filter.set_regex_pattern(arg[0], re.IGNORECASE)
            else:
                print("Controller.set_filter: unknown command: {}".format(command))
        else:
            self.filter.set_pattern(pattern)

        self.apply_filter()

    def go_forward(self):
        if self.history != []:
            self.history_index = min(self.history_index + 1, len(self.history) - 1)
            self.history[self.history_index]
            self.set_location(self.history[self.history_index], track_history=False)
            if self.history_index == len(self.history) - 1:
                self.actions.forward.setEnabled(False)
            self.actions.back.setEnabled(True)

    def go_back(self):
        if self.history != []:
            self.history_index = max(self.history_index - 1, 0)
            self.set_location(self.history[self.history_index], track_history=False)
            if self.history_index == 0:
                self.actions.back.setEnabled(False)
            self.actions.forward.setEnabled(True)

    def go_home(self):
        home = os.path.expanduser("~")
        self.set_location(home)

    def set_location(self, location, track_history=True):
        if track_history:
            self.history = self.history[0:self.history_index + 1]
            self.history_index = len(self.history)
            self.history.append(location)
            self.actions.back.setEnabled(True)
            self.actions.forward.setEnabled(False)


        # Work in progress
        self.file_collection.clear()

        if self.directory_watcher_thread is not None:
            self.directory_watcher._close = True
            self.directory_watcher.sig_close_requested.emit()

        self.directory_watcher_thread = QThread(self)
        self.directory_watcher = DirectoryWatcher(location)
        self.directory_watcher.moveToThread(self.directory_watcher_thread)

        if False:
            self.directory_watcher.sig_file_added.connect(lambda x: print("added", x))
            self.directory_watcher.sig_file_removed.connect(lambda x: print("removed", x))
            self.directory_watcher.sig_file_changed.connect(lambda x: print("changed", x))
            self.directory_watcher.sig_finished.connect(self.directory_watcher_thread.quit)

        self.directory_watcher.sig_file_added.connect(self.file_collection.add_fileinfo)
        self.directory_watcher.sig_file_removed.connect(self.file_collection.remove_file)
        # self.directory_watcher.sig_file_changed.connect(lambda x: print("changed", x))
        self.directory_watcher.sig_scandir_finished.connect(self.on_scandir_finished)

        self.directory_watcher_thread.started.connect(self.directory_watcher.process)
        self.directory_watcher_thread.finished.connect(self.directory_watcher_thread.deleteLater)
        self.directory_watcher_thread.start()


        self.location = os.path.abspath(location)
        self.window.set_location(self.location)

        # files = expand_file(self.location, recursive=False)
        # self.set_files(files, self.location)

    def on_scandir_finished(self, fileinfos):
        self.file_collection.set_fileinfos(fileinfos)
        self.apply_sort()
        self.apply_filter()

    def set_files(self, files, location=None):
        if location is None:
            self.window.set_file_list()
        self.location = location
        self.file_collection.set_files(files)
        self.apply_sort()
        self.apply_filter()

    def apply_sort(self):
        logging.debug("Controller.apply_sort")
        self.sorter.apply(self.file_collection)

    def apply_filter(self):
        logging.debug("Controller.apply_filter")
        self.file_collection.filter(self.filter)

        fileinfos = self.file_collection.get_fileinfos()

        filtered_count = 0
        hidden_count = 0
        for fileinfo in fileinfos:
            if fileinfo.is_hidden:
                hidden_count += 1
            elif fileinfo.is_filtered:
                filtered_count += 1

        total = self.file_collection.size()

        self.window.show_info("{} visible, {} filtered, {} hidden, {} total".format(
            total - filtered_count - hidden_count,
            filtered_count,
            hidden_count,
            total))

        self.window.thumb_view.set_filtered(filtered_count > 0)

    def toggle_timegaps(self):
        self.window.file_view.toggle_timegaps()

    def parent_directory(self):
        if self.location is not None:
            self.set_location(os.path.dirname(os.path.abspath(self.location)))

    def on_click(self, fileinfo, new_window=False):
        if not fileinfo.isdir():
            argv = ["xdg-open", fileinfo.filename()]
            logging.info("Controller.on_click: launching: %s", argv)
            subprocess.Popen(argv)
        else:
            if self.location is None or new_window:
                logging.info("Controller.on_click: app.show_location: %s", fileinfo)
                self.app.show_location(fileinfo.filename())
            else:
                logging.info("Controller.on_click: self.set_location: %s", fileinfo)
                self.set_location(fileinfo.filename())

    def show_current_filename(self, filename):
        self.window.show_current_filename(filename)

    def add_files(self, files):
        for f in files:
            self.file_collection.add_file(f)

    def request_thumbnail(self, fileinfo, flavor):
        self.app.thumbnailer.request_thumbnail(fileinfo.filename(), flavor,
                                               self.receive_thumbnail)

    def reload(self):
        self.window.thumb_view.reload()

    def receive_thumbnail(self, filename, flavor, pixmap):
        self.window.thumb_view.receive_thumbnail(filename, flavor, pixmap)

    def reload_thumbnails(self):
        self.app.dbus_thumbnail_cache.delete(
            [f.abspath()
             for f in self.file_collection.get_fileinfos()])
        self.window.thumb_view.reload_thumbnails()


# EOF #
