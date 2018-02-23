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


from typing import List, Optional

import logging
import random

from PyQt5.QtCore import QObject, pyqtSignal

from dirtools.fileview.location import Location
from dirtools.fileview.file_info import FileInfo

logger = logging.getLogger(__name__)


class FileCollection(QObject):

    # A new file entry has been added
    sig_file_added = pyqtSignal(FileInfo)

    # An existing file entry has been removed
    sig_file_removed = pyqtSignal(Location)

    # A file changed on disk
    sig_file_changed = pyqtSignal(FileInfo)

    # New information about a file has becomes available (thumbnail,
    # metadata, etc.)
    sig_file_updated = pyqtSignal(FileInfo)

    # The file list has changed completely and needs a reload from scratch
    sig_files_set = pyqtSignal()

    # The file list has been reordered, but otherwised stayed the same
    sig_files_reordered = pyqtSignal()

    # The file list has been filtered, .is_excluded has been
    # set/unset, but otherwise stayed the same
    sig_files_filtered = pyqtSignal()

    # The file list has been grouped, .group has been set
    sig_files_grouped = pyqtSignal()

    def __init__(self) -> None:
        super().__init__()
        self.fileinfos: List[FileInfo] = []

    def clear(self) -> None:
        logger.debug("FileCollection.clear")
        self.fileinfos = []
        self.sig_files_set.emit()

    def set_files(self, files: List[Location]) -> None:
        logger.debug("FileCollection.set_files")
        self.fileinfos = [FileInfo.from_location(f) for f in files]
        self.sig_files_set.emit()

    def set_fileinfos(self, fileinfos: List[FileInfo]) -> None:
        logger.debug("FileCollection.set_fileinfos")
        self.fileinfos = fileinfos
        self.sig_files_set.emit()

    def add_fileinfo(self, fi: FileInfo):
        logger.debug("FileCollection.add_fileinfos: %s", fi)
        self.fileinfos.append(fi)
        self.sig_file_added.emit(fi)

    def add_file(self, location: Location) -> None:
        logger.debug("FileCollection.add_file: %s", location)
        fi = FileInfo.from_location(location)
        self.fileinfos.append(fi)
        self.sig_file_added.emit(fi)

    def remove_file(self, location: Location) -> None:
        logger.debug("FileCollection.remove_file: %s", location)
        self.fileinfos = [fi for fi in self.fileinfos if fi.location() == location]
        self.sig_file_removed.emit(location)

    def change_file(self, fileinfo: FileInfo) -> None:
        logger.debug("FileCollection.change_file: %s", fileinfo)
        # We assume here that the supplied FileInfo is identical to
        # one already in our storage
        self.sig_file_changed.emit(fileinfo)

    def update_file(self, fileinfo: FileInfo) -> None:
        logger.debug("FileCollection.change_file: %s", fileinfo)
        # We assume here that the supplied FileInfo is identical to
        # one already in our storage
        self.sig_file_updated.emit(fileinfo)

    def get_fileinfos(self) -> List[FileInfo]:
        return self.fileinfos

    def get_fileinfo(self, abspath: str) -> Optional[FileInfo]:
        for fi in self.fileinfos:
            if fi.abspath() == abspath:
                return fi
        return None

    def size(self) -> int:
        return len(self.fileinfos)

    def group(self, grouper):
        grouper.apply(self.fileinfos)
        self.sig_files_grouped.emit()

    def filter(self, filter):
        filter.apply(self.fileinfos)
        self.sig_files_filtered.emit()

    def sort(self, key, reverse=False):
        logger.debug("FileCollection.sort")
        self.fileinfos = sorted(self.fileinfos, key=key)
        if reverse:
            self.fileinfos = list(reversed(self.fileinfos))
        logger.debug("FileCollection.sort:done")
        self.sig_files_reordered.emit()

    def shuffle(self):
        logger.debug("FileCollection.sort")
        random.shuffle(self.fileinfos)
        logger.debug("FileCollection.sort:done")
        self.sig_files_reordered.emit()

    def save_as(self, filename: str):
        with open(filename, "w") as fout:
            for fi in self.fileinfos:
                fout.write(fi.abspath)
                fout.write("\n")


# EOF #
