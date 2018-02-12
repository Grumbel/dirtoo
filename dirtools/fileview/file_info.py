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


from typing import Union, Dict, Any
import logging

import os
import stat

from dirtools.fileview.filter_parser import VIDEO_EXT, IMAGE_EXT

logger = logging.getLogger(__name__)


class FileInfo:

    @staticmethod
    def from_direntry(direntry) -> 'FileInfo':
        logger.debug("FileInfo.from_direntry: %s/%s", direntry.path, direntry.name)

        fi = FileInfo()
        fi._abspath = os.path.abspath(direntry.path)
        fi._dirname = os.path.dirname(fi._abspath)
        fi._basename = direntry.name
        fi._ext = os.path.splitext(fi._abspath)[1]

        fi._isdir = direntry.is_dir()
        fi._isfile = direntry.is_file()
        fi._issymlink = direntry.is_symlink()

        fi._collect_stat()

        return fi

    @staticmethod
    def from_filename(filename: str) -> 'FileInfo':
        logger.debug("FileInfo.from_filename: %s", filename)

        fi = FileInfo()
        fi._abspath = os.path.abspath(filename)
        fi._dirname = os.path.dirname(fi._abspath)
        fi._basename = os.path.basename(fi._abspath)
        fi._ext = os.path.splitext(fi._abspath)[1]

        fi._collect_stat()

        fi._isdir = os.path.isdir(fi._abspath)
        fi._isfile = stat.S_ISREG(fi._stat.st_mode)
        fi._issymlink = stat.S_ISLNK(fi._stat.st_mode)

        return fi

    def __init__(self) -> None:
        self._abspath: Union[str, None] = None
        self._dirname: Union[str, None] = None
        self._basename: Union[str, None] = None
        self._ext: Union[str, None] = None

        self._isdir: Union[bool, None] = None
        self._isfile: Union[bool, None] = None
        self._issymlink: Union[bool, None] = None

        self._stat: Union[os.stat_result, None] = None
        self._have_access: Union[bool, None] = None

        self._metadata: Dict[str, Any] = {}

        self.is_excluded: bool = False
        self.is_hidden: bool = False

    def _collect_stat(self) -> None:
        self._stat = os.lstat(self._abspath)
        self._have_access = os.access(self._abspath, os.R_OK)

    @property
    def is_visible(self) -> bool:
        return not self.is_hidden and not self.is_excluded

    def have_access(self) -> bool:
        return self._have_access

    def abspath(self) -> str:
        return self._abspath

    def dirname(self) -> str:
        return self._dirname

    def basename(self) -> str:
        return self._basename

    def isdir(self):
        return self._isdir

    def is_thumbnailable(self):
        return self.is_video() or self.is_image()

    def is_video(self):
        return self._ext[1:] in VIDEO_EXT

    def is_image(self):
        return self._ext[1:] in IMAGE_EXT

    def stat(self):
        return self._stat

    def uid(self):
        return self._stat.st_uid

    def gid(self):
        return self._stat.st_gid

    def ext(self):
        return self._ext

    def size(self):
        return self._stat.st_size if self._stat is not None else None

    def mtime(self):
        return self._stat.st_mtime if self._stat is not None else None

    def metadata(self):
        return self._metadata

    def __str__(self):
        return "FileInfo({})".format(self._abspath)


# EOF #
