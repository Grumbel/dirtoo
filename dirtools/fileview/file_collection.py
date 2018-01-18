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


import logging
import random
from typing import List

from PyQt5.QtCore import QObject, pyqtSignal
from dirtools.fileview.file_info import FileInfo


class FileCollection(QObject):

    # A single file entry has been added or removed
    sig_file_added = pyqtSignal(FileInfo)
    sig_file_removed = pyqtSignal(FileInfo)

    # The file list has changed completely and needs a reload from scratch
    sig_files_set = pyqtSignal()

    # The file list has been reordered, but otherwised stayed the same
    sig_files_reordered = pyqtSignal()

    # The file list has been filtered, .is_filtered has been
    # set/unset, but otherwise stayed the same
    sig_files_filtered = pyqtSignal()

    def __init__(self, files=None):
        super().__init__()
        if files is None:
            self.fileinfos: List[FileInfo] = []
        else:
            self.fileinfos = [FileInfo(f) for f in files]

    def set_files(self, files):
        self.fileinfos = [FileInfo(f) for f in files]
        self.sig_files_set.emit()

    def add_file(self, filename):
        fi = FileInfo(filename)
        self.fileinfos.append(fi)
        self.sig_file_added.emit(fi)

    def remove_file(self, filename):
        self.fileinfoss = [fi for fi in self.files if fi.abspath == filename]
        self.sig_file_removed.emit(filename)

    def get_fileinfos(self):
        return self.fileinfos

    def size(self):
        return len(self.fileinfos)

    def filter(self, filter_func):
        for fileinfo in self.fileinfos:
            fileinfo.is_filtered = filter_func(fileinfo)
        self.sig_files_filtered.emit()

    def sort(self, key, reverse=False):
        logging.debug("FileCollection.sort")
        self.fileinfos = sorted(self.fileinfos, key=key)
        if reverse:
            self.fileinfos = list(reversed(self.fileinfos))
        logging.debug("FileCollection.sort:done")
        self.sig_files_reordered.emit()

    def shuffle(self):
        logging.debug("FileCollection.sort")
        random.shuffle(self.fileinfos)
        logging.debug("FileCollection.sort:done")
        self.sig_files_reordered.emit()

    def save_as(self, filename):
        with open(filename, "w") as fout:
            for fi in self.fileinfos:
                fout.write(fi.abspath)
                fout.write("\n")


# EOF #
