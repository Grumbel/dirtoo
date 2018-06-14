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


from typing import Dict, Any, Optional
import logging

import os
import stat

from dirtools.fileview.location import Location
from dirtools.fileview.match_func_factory import VIDEO_EXT, IMAGE_EXT, ARCHIVE_EXT

logger = logging.getLogger(__name__)


class LazyFileInfo:

    @staticmethod
    def from_path(path: str) -> 'LazyFileInfo':
        logger.debug("LazyFileInfo.from_path: %s", path)

        fi = LazyFileInfo(path)
        return fi

    def __init__(self, path) -> None:
        self._abspath: str = os.path.abspath(path)

        self._location: Optional[Location] = None

        self._dirname: str = os.path.dirname(self._abspath)
        self._basename: str = os.path.basename(self._abspath)
        self._ext: str = os.path.splitext(self._abspath)[1]

        self._isdir: Optional[bool] = None
        self._isfile: Optional[bool] = None

        self._stat: Optional[os.stat_result] = None
        self._have_access: Optional[bool] = None

        self._filenotfound = False

        self._metadata: Optional[Dict[str, Any]] = None

    def _collect_stat(self) -> None:
        if self._stat is None:
            self._stat = os.lstat(self._abspath)
            self._have_access = os.access(self._abspath, os.R_OK)

    def have_access(self) -> bool:
        self._collect_stat()
        return self._have_access

    def abspath(self) -> str:
        return self._abspath

    def location(self) -> Location:
        if self._location is None:
            self._location = Location.from_path(self._abspath)

        return self._location

    def dirname(self) -> str:
        return self._dirname

    def basename(self) -> str:
        return self._basename

    def isdir(self) -> bool:
        return os.path.isdir(self._abspath)

    def isfile(self) -> bool:
        self._collect_stat()
        return stat.S_ISREG(self._stat.st_mode)

    def is_video(self) -> bool:
        return self._ext[1:].lower() in VIDEO_EXT

    def is_image(self) -> bool:
        return self._ext[1:].lower() in IMAGE_EXT

    def is_archive(self) -> bool:
        return self._ext[1:].lower() in ARCHIVE_EXT

    def stat(self) -> os.stat_result:
        self._collect_stat()
        return self._stat

    def uid(self) -> int:
        self._collect_stat()
        return self._stat.st_uid

    def gid(self) -> int:
        self._collect_stat()
        return self._stat.st_gid

    def ext(self) -> str:
        return self._ext

    def size(self) -> int:
        self._collect_stat()
        return self._stat.st_size

    def mtime(self) -> float:
        self._collect_stat()
        return self._stat.st_mtime

    def metadata(self) -> Dict[str, Any]:
        return {}  # FIXME: fetch metadata

    def __str__(self) -> str:
        return "LazyFileInfo({})".format(self._abspath)


# EOF #
