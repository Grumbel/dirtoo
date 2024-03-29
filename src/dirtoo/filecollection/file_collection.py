# dirtoo - File and directory manipulation tools for Python
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


from typing import cast, overload, Iterable, Optional, Dict, Sequence, Union

import logging

from collections import defaultdict
from sortedcontainers import SortedList

from PyQt6.QtCore import QObject, pyqtSignal

from dirtoo.filesystem.file_info import FileInfo
from dirtoo.filecollection.filter import Filter
from dirtoo.filecollection.grouper import Grouper, NoGrouper
from dirtoo.filecollection.sorter import Sorter
from dirtoo.filesystem.location import Location

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

        self._location2fileinfo: Dict[Location, list[FileInfo]] = defaultdict(list)
        self._fileinfos: SortedList[FileInfo] = SortedList(key=self._sorter.get_key_func())

    def clear(self) -> None:
        logger.debug("FileCollection.clear")

        self._location2fileinfo.clear()
        self._fileinfos.clear()

        self.sig_files_set.emit()

    def set_fileinfos(self, fileinfos_iter: Iterable[FileInfo]) -> None:
        logger.debug("FileCollection.set_fileinfos")

        # force to list, as we iterate over it twice
        fileinfos = list(fileinfos_iter)

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

    def update_metadata(self, location: Location, metadata: Dict[str, object]) -> None:
        fileinfo = self.get_fileinfo(location)
        if fileinfo is None:
            logger.error("Controller.receive_metadata: not found fileinfo for %s", location)
            return

        # FIXME: filedata must not be updated while it is inside
        # SortedList, as otherwise SortedList corrupts due to the sort
        # order being invalid. This is an ugly workaround
        self._fileinfos.remove(fileinfo)
        fileinfo._metadata.update(metadata)
        self._fileinfos.add(fileinfo)

        self.sig_fileinfo_updated.emit(fileinfo)

    def close_file(self, fileinfo: FileInfo) -> None:
        try:
            self._replace_fileinfo(fileinfo)
        except KeyError:
            logger.error("FileCollection.close_file: %s", fileinfo)
        else:
            logger.debug("FileCollection.close_file: %s: KeyError", fileinfo)
            self.sig_file_closed.emit(fileinfo)

    def get_fileinfos(self) -> Sequence[FileInfo]:
        if self._sorter.reverse:
            return list(reversed(self._fileinfos))
        else:
            return list(self._fileinfos)

    def get_fileinfo(self, location: Location) -> Optional[FileInfo]:
        if location not in self._location2fileinfo:
            return None
        else:
            fis = self._location2fileinfo[location]
            return fis[0]  # FIXME: this is fishy

    def index(self, fileinfo: FileInfo) -> int:
        return cast(int, self._fileinfos.index(fileinfo))

    @overload
    def __getitem__(self, key: int) -> FileInfo:
        ...

    @overload
    def __getitem__(self, key: slice) -> list[FileInfo]:
        ...

    def __getitem__(self, key: Union[int, slice]) -> Union[FileInfo, list[FileInfo]]:
        return cast(Union[FileInfo, list[FileInfo]],
                    self._fileinfos[key])

    def __len__(self) -> int:
        return len(self._fileinfos)

    def set_grouper(self, grouper: Grouper) -> None:
        self._grouper = grouper

        for fi in self._fileinfos:
            self._grouper(fi)

        self.sig_files_grouped.emit()

    def set_filter(self, filt: Filter) -> None:
        self._filter = filt

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

        fis = self._location2fileinfo[location]

        self._location2fileinfo[location] = [fileinfo]

        for fi in fis:
            self._fileinfos.remove(fi)

        self._fileinfos.add(fileinfo)

    def verify(self) -> None:
        for item in self._fileinfos:
            print(item)
        print("------------------")
        for k, v in self._location2fileinfo.items():
            print(f"{k} {v}")
        print("------------------")
        for loc, fis in self._location2fileinfo.items():
            for fi in fis:
                self._fileinfos.index(fi)

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
