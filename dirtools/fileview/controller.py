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


import subprocess
from fnmatch import fnmatch

from PyQt5.QtWidgets import QFileDialog
from PyQt5.QtCore import QObject
from dirtools.fileview.actions import Actions
from dirtools.fileview.file_collection import FileCollection
from dirtools.fileview.file_view_window import FileViewWindow


class Controller(QObject):

    def __init__(self, thumbnailer):
        super().__init__()
        self.thumbnailer = thumbnailer
        self.file_collection = FileCollection()
        self.actions = Actions(self)
        self.window = FileViewWindow(self)

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
            files = self.file_collection.fileinfos
        else:
            files = [f for f in self.file_collection.fileinfos
                     if fnmatch(f.basename, pattern)]

        self.window.show_info("{} items, {} filtered, {} total".format(
            len(files),
            self.file_collection.size() - len(files),
            self.file_collection.size()))

        # FIXME: self.window.file_view.set_files([f.filename for f in files])
        # FIXME: self.window.thumb_view.set_files([f.filename for f in files])

    def set_files(self, files):
        self.file_collection.set_files(files)
        self.file_collection.sort(lambda fileinfo:
                                  (not fileinfo.isdir, fileinfo.basename))
        self.window.show_info("{} items".format(len(files)))
        self.window.file_view.set_file_collection(self.file_collection)
        self.window.thumb_view.set_file_collection(self.file_collection)

    def toggle_timegaps(self):
        self.window.file_view.toggle_timegaps()

    def parent_directory(self):
        pass

    def on_click(self, fileinfo):
        subprocess.Popen(["xdg-open", fileinfo.filename])

    def show_current_filename(self, filename):
        self.window.show_current_filename(filename)

    def add_files(self, files):
        for f in files:
            self.file_collection.add_file(f)

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
