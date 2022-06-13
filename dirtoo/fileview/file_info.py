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


from typing import Dict, Any, Optional
import logging

import os
import stat
import errno
from enum import Enum

from dirtoo.fileview.location import Location
from dirtoo.fileview.match_func_factory import VIDEO_EXT, IMAGE_EXT, ARCHIVE_EXT

logger = logging.getLogger(__name__)


class FileInfoError(Enum):

    NO_ERROR = 0
    FILENOTFOUND = 1
    PERMISSIONDENIED = 2
    IO = 3
    UNKNOWN = 4


class FileInfo:

    @staticmethod
    def from_path(path: str) -> 'FileInfo':
        logger.debug("FileInfo.from_path: %s", path)

        fi = FileInfo()

        fi._abspath = os.path.abspath(path)
        fi._location = Location.from_path(fi._abspath)
        fi._dirname = os.path.dirname(fi._abspath)
        fi._basename = os.path.basename(fi._abspath)
        fi._ext = os.path.splitext(fi._abspath)[1]

        try:
            fi._stat = os.lstat(fi._abspath)
            fi._have_access = os.access(fi._abspath, os.R_OK)
            fi._error = FileInfoError.NO_ERROR
        except (FileNotFoundError, NotADirectoryError):
            fi._error = FileInfoError.FILENOTFOUND
        except PermissionError:
            fi._error = FileInfoError.PERMISSIONDENIED
        except OSError as err:
            if err.errno == errno.EIO:
                fi._error = FileInfoError.IO
            else:
                fi._error = FileInfoError.UNKNOWN
        except Exception:
            fi._error = FileInfoError.UNKNOWN
        else:
            fi._isdir = os.path.isdir(fi._abspath)
            fi._isfile = stat.S_ISREG(fi._stat.st_mode)
            fi._issymlink = stat.S_ISLNK(fi._stat.st_mode)

        return fi

    def __init__(self) -> None:
        self._abspath: str = ""
        self._location: Optional[Location] = None
        self._dirname: str = ""
        self._basename: str = ""
        self._ext: str = ""

        self._isdir: bool = False
        self._isfile: bool = False
        self._issymlink: bool = False

        self._stat: Optional[os.stat_result] = None
        self._have_access: bool = False

        self._error: FileInfoError = FileInfoError.NO_ERROR

        self._metadata: Dict[str, Any] = {}

        # filter variables
        self.is_excluded: bool = False
        self.is_hidden: bool = False

        # grouper variables
        self.group: Any = None

    @property
    def is_visible(self) -> bool:
        return not self.is_hidden and not self.is_excluded

    def error(self) -> FileInfoError:
        return self._error

    def have_access(self) -> bool:
        return self._have_access

    def abspath(self) -> str:
        return self._abspath

    def location(self) -> Location:
        assert self._location is not None
        return self._location

    def dirname(self) -> str:
        return self._dirname

    def basename(self) -> str:
        return self._basename

    def isdir(self) -> bool:
        return self._isdir

    def isfile(self) -> bool:
        return self._isfile

    def is_video(self) -> bool:
        return self._ext[1:].lower() in VIDEO_EXT

    def is_image(self) -> bool:
        return self._ext[1:].lower() in IMAGE_EXT

    def is_archive(self) -> bool:
        return self._ext[1:].lower() in ARCHIVE_EXT

    def stat(self) -> os.stat_result:
        return self._stat

    def uid(self) -> int:
        return self._stat.st_uid if self._stat else 0

    def gid(self) -> int:
        return self._stat.st_gid if self._stat else 0

    def ext(self) -> str:
        return self._ext

    def size(self) -> int:
        return self._stat.st_size if self._stat is not None else 0

    def atime(self) -> float:
        return self._stat.st_atime if self._stat is not None else 0

    def ctime(self) -> float:
        return self._stat.st_ctime if self._stat is not None else 0

    def mtime(self) -> float:
        return self._stat.st_mtime if self._stat is not None else 0

    def metadata(self) -> Dict[str, Any]:
        return self._metadata

    def __str__(self) -> str:
        return "FileInfo({})".format(self._location)


# EOF #
