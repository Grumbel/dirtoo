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


import os
import subprocess

from PyQt5.QtWidgets import QFileDialog
from PyQt5.QtCore import QObject
from dirtools.fileview.actions import Actions
from dirtools.fileview.file_collection import FileCollection
from dirtools.fileview.file_view_window import FileViewWindow
from dirtools.util import expand_file
from dirtools.fileview.filter import Filter
from dirtools.fileview.sorter import Sorter


class Controller(QObject):

    def __init__(self, app):
        super().__init__()
        self.app = app
        self.file_collection = FileCollection()
        self.actions = Actions(self)
        self.window = FileViewWindow(self)
        self.filter = Filter()
        self.sorter = Sorter(self)
        self.history = []
        self.history_index = 0

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
        self.refresh()

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

    def set_filter(self, pattern):
        if pattern == "":
            self.filter.pattern = None
        else:
            self.filter.pattern = pattern
        self.refresh()

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

    def set_location(self, location, track_history=True):
        if track_history:
            self.history = self.history[0:self.history_index + 1]
            self.history_index = len(self.history)
            self.history.append(location)
            self.actions.back.setEnabled(True)
            self.actions.forward.setEnabled(False)

        self.location = os.path.abspath(location)
        self.window.set_location(self.location)
        files = expand_file(self.location, recursive=False)
        self.set_files(files, self.location)

    def set_files(self, files, location=None):
        self.location = location
        self.file_collection.set_files(files)
        self.apply_sort()
        self.refresh()

    def apply_sort(self):
        self.sorter.apply(self.file_collection)

    def apply_filter(self):
        self.filter.apply(self.file_collection)

    def toggle_timegaps(self):
        self.window.file_view.toggle_timegaps()

    def parent_directory(self):
        if self.location is not None:
            self.set_location(os.path.dirname(os.path.abspath(self.location)))

    def on_click(self, fileinfo):
        if not fileinfo.isdir():
            subprocess.Popen(["xdg-open", fileinfo.filename()])
        else:
            if self.location is None:
                self.app.show_location(fileinfo.filename())
            else:
                self.set_location(fileinfo.filename())

    def show_current_filename(self, filename):
        self.window.show_current_filename(filename)

    def add_files(self, files):
        for f in files:
            self.file_collection.add_file(f)

    def refresh(self):
        self.filter.apply(self.file_collection)

        fileinfos = self.file_collection.get_fileinfos()
        filtered_count = 0
        for fileinfo in fileinfos:
            if fileinfo.is_filtered:
                filtered_count += 1

        total = self.file_collection.size()

        self.window.show_info("{} items, {} filtered, {} total".format(
            filtered_count,
            total - filtered_count,
            total))

        self.window.file_view.set_file_collection(self.file_collection)
        self.window.thumb_view.set_file_collection(self.file_collection)

    def reload(self):
        self.window.thumb_view.reload()

    # Temp Hacks
    @property
    def tn_width(self):
        return self.window.thumb_view.tn_width

    @property
    def tn_height(self):
        return self.window.thumb_view.tn_height

    @property
    def tn_size(self):
        return self.window.thumb_view.tn_size

    @property
    def flavor(self):
        return self.window.thumb_view.flavor


# EOF #
