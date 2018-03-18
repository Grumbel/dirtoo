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

from dirtools.fileview.location import Location
from dirtools.fileview.file_info import FileInfo
from dirtools.fileview.stdio_filesystem import StdioFilesystem
from dirtools.fileview.directory_watcher import DirectoryWatcher

logger = logging.getLogger(__name__)


class VirtualFilesystem:

    def __init__(self, cachedir: str) -> None:
        self._stdio_fs = StdioFilesystem(cachedir)

    def close(self) -> None:
        self._stdio_fs.close()

    def opendir(self, location: Location) -> DirectoryWatcher:
        return self._stdio_fs.opendir(location)

    def get_fileinfo(self, location: Location) -> FileInfo:
        return self._stdio_fs.get_fileinfo(location)

    def get_stdio_url(self, location: Location) -> str:
        return self._stdio_fs.get_stdio_url(location)

    def get_stdio_name(self, location: Location) -> str:
        return self._stdio_fs.get_stdio_name(location)


# EOF #
