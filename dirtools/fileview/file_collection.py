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


from typing import Iterable, Optional, Iterator, Dict, List, cast

import logging

from collections import defaultdict
from sortedcollections import SortedList

from PyQt5.QtCore import QObject, pyqtSignal

from dirtools.fileview.location import Location
from dirtools.fileview.file_info import FileInfo
from dirtools.fileview.filter import Filter
from dirtools.fileview.grouper import Grouper, NoGrouper
from dirtools.fileview.sorter import Sorter

logger = logging.getLogger(__name__)


class FileCollection(QObject):

    # A new file entry has been added
    sig_file_added = pyqtSignal(int, FileInfo)

    # An existing file entry has been removed
    sig_file_removed = pyqtSignal(Location)

    # A file changed on disk
    sig_file_modified = pyqtSignal(FileInfo)

    # New information about a file has becomes available (thumbnail,
    # metadata, etc.)
    sig_fileinfo_updated = pyqtSignal(FileInfo)

    # File handle was closed and the file is in it's final state.
    sig_file_closed = pyqtSignal(FileInfo)

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

        self._grouper: Grouper = NoGrouper()
        self._filter: Filter = Filter()
        self._sorter: Sorter = Sorter()

        self._location2fileinfo: Dict[Location, List[FileInfo]] = defaultdict(list)
        self._fileinfos: SortedList[FileInfo] = SortedList(key=self._sorter.get_key_func())

    def clear(self) -> None:
        logger.debug("FileCollection.clear")

        self._location2fileinfo.clear()

        self._fileinfos.clear()

        self.sig_files_set.emit()

    def set_fileinfos(self, fileinfos: Iterable[FileInfo]) -> None:
        logger.debug("FileCollection.set_fileinfos")

        for fi in fileinfos:
            self._location2fileinfo[fi.location()].append(fi)

        self._fileinfos.clear()
        self._fileinfos.update(fileinfos)

        self.sig_files_set.emit()

    def add_fileinfo(self, fi: FileInfo) -> None:
        logger.debug("FileCollection.add_fileinfos: %s", fi)

        self._location2fileinfo[fi.location()].append(fi)

        self._fileinfos.add(fi)

        idx = self._fileinfos.index(fi)
        self.sig_file_added.emit(idx, fi)

    def remove_file(self, location: Location) -> None:
        if location not in self._location2fileinfo:
            logger.error("FileCollection.remove_file: %s: KeyError", location)
        else:
            logger.debug("FileCollection.remove_file: %s", location)
            fis = self._location2fileinfo[location]

            del self._location2fileinfo[location]
            for fi in fis:
                self._fileinfos.remove(fi)

            self.sig_file_removed.emit(location)

    def modify_file(self, fileinfo: FileInfo) -> None:
        try:
            self._replace_fileinfo(fileinfo)
        except KeyError:
            logger.error("FileCollection.modify_file: %s: KeyError", fileinfo)
        else:
            logger.debug("FileCollection.modify_file: %s", fileinfo)
            self.sig_file_modified.emit(fileinfo)

    def update_fileinfo(self, fileinfo: FileInfo) -> None:
        try:
            self._replace_fileinfo(fileinfo)
        except KeyError:
            logger.error("FileCollection.update_fileinfo: %s", fileinfo)
        else:
            logger.debug("FileCollection.update_fileinfo: %s: KeyError", fileinfo)
            self.sig_fileinfo_updated.emit(fileinfo)

    def close_file(self, fileinfo: FileInfo) -> None:
        try:
            self._replace_fileinfo(fileinfo)
        except KeyError:
            logger.error("FileCollection.close_file: %s", fileinfo)
        else:
            logger.debug("FileCollection.close_file: %s: KeyError", fileinfo)
            self.sig_file_closed.emit(fileinfo)

    def get_fileinfos(self) -> Iterator[FileInfo]:
        if self._sorter.reverse:
            return cast(Iterator[FileInfo], reversed(self._fileinfos))
        else:
            return cast(Iterator[FileInfo], iter(self._fileinfos))

    def get_fileinfo(self, location: Location) -> Optional[FileInfo]:
        if location not in self._location2fileinfo:
            return None
        else:
            fis = self._location2fileinfo[location]
            return fis[0]  # FIXME: this is fishy

    def index(self, fileinfo: FileInfo):
        return self._fileinfos.index(fileinfo)

    def __getitem__(self, key):
        return self._fileinfos[key]

    def __len__(self) -> int:
        return len(self._fileinfos)

    def set_grouper(self, grouper: Grouper) -> None:
        self._grouper = grouper

        for fi in self._fileinfos:
            self._grouper(fi)

        self.sig_files_grouped.emit()

    def set_filter(self, filter: Filter) -> None:
        self._filter = filter

        for fi in self._fileinfos:
            self._filter.apply(fi)

        self.sig_files_filtered.emit()

    def set_sorter(self, sorter: Sorter) -> None:
        print("rebuilding fileinfos")
        self._sorter = sorter
        self._fileinfos = SortedList(self._fileinfos, key=self._sorter.get_key_func())
        self.sig_files_reordered.emit()

    # def sort(self, key, reverse: bool=False) -> None:
    #     logger.debug("FileCollection.sort")
    #     self._fileinfos.sort(key=key)
    #     if reverse:
    #         self._fileinfos.sort(key=key, reverse=True)
    #     logger.debug("FileCollection.sort:done")
    #     self.sig_files_reordered.emit()

    def _replace_fileinfo(self, fileinfo: FileInfo) -> None:
        if fileinfo in self._fileinfos:
            return

        location = fileinfo.location()

        if location not in self._location2fileinfo:
            raise KeyError("location not in location2fileinfo: {}".format(location))
        else:
            fis = self._location2fileinfo[location]

            self._location2fileinfo[location] = [fileinfo]

            for fi in fis:
                self._fileinfos.remove(fi)

            self._fileinfos.add(fileinfo)

    # def shuffle(self) -> None:
    #     logger.debug("FileCollection.sort")

    #     tmp = list(self._fileinfos)
    #     random.shuffle(tmp)
    #     self._fileinfos = ListDict(lambda fi: fi.location(), tmp)

    #     logger.debug("FileCollection.sort:done")
    #     self.sig_files_reordered.emit()

    def save_as(self, filename: str) -> None:
        with open(filename, "w") as fout:
            for fi in self._fileinfos:
                fout.write(fi.abspath())
                fout.write("\n")


# EOF #
