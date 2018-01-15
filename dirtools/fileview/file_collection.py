# dirtool.py - diff tool for directories
# Copyright (C) 2018 Ingo Ruhnke <grumbel@gmail.com>
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

from PyQt5.QtCore import QObject, pyqtSignal
from dirtools.fileview.file_info import FileInfo


class FileCollection(QObject):

    sig_file_added = pyqtSignal(FileInfo)
    sig_file_removed = pyqtSignal(FileInfo)
    sig_files_changed = pyqtSignal(list)  # [FileInfo]

    def __init__(self):
        super().__init__()
        self.fileinfos: List[FileInfo] = []

    def set_files(self, files):
        self.fileinfos = [FileInfo(f) for f in files]
        self.sig_files_changed.emit(self.fileinfos)

    def add_file(self, filename):
        fi = FileInfo(filename)
        self.fileinfos.append(fi)
        self.sig_file_added.emit(fi)

    def remove_file(self, filename):
        self.fileinfoss = [fi for fi in self.files if fi.abspath == filename]
        self.sig_file_removed.emit(filename)

    def size(self):
        return len(self.fileinfos)

    def save_as(self, filename):
        with open(filename, "w") as fout:
            for fi in self.fileinfos:
                fout.write(fi.abspath)
                fout.write("\n")

    def sort(self, key):
        self.fileinfos = sorted(self.fileinfos, key=key)
        self.sig_files_changed.emit(self.fileinfos)


# EOF #
