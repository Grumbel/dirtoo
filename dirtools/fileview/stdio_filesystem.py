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


import os
import logging
import urllib

from dirtools.fileview.location import Location
from dirtools.fileview.directory_watcher import DirectoryWatcher
from dirtools.archive_manager import ArchiveManager
from dirtools.fileview.file_info import FileInfo

logger = logging.getLogger(__name__)


class StdioFilesystem:

    def __init__(self, cachedir: str) -> None:
        self.cachedir = cachedir
        self._archive_manager = ArchiveManager(os.path.join(self.cachedir, "extractors"))

    def close(self) -> None:
        self._archive_manager.close()

    def opendir(self, location: Location) -> 'DirectoryWatcher':
        """Create a watcher object for the given directory. The caller is
        responsible for .close()`ing it."""

        if not location.has_payload():
            directory_watcher = DirectoryWatcher(self, location)
            return directory_watcher
        else:
            directory_watcher = DirectoryWatcher(self, location)
            self._archive_manager.extract(location, directory_watcher, self)
            return directory_watcher

    def get_fileinfo(self, location: Location) -> FileInfo:
        if not location.has_payload():
            fi = FileInfo.from_path(location.get_path())
            fi._location = location
            return fi
        else:
            parent = location.parent()
            assert parent.has_payload()

            outdir = self._archive_manager.get_extractor_content_dir(parent)
            path = os.path.join(outdir, location._payloads[-1].path)

            fi = FileInfo.from_path(path)
            fi._location = location
            return fi

    def get_stdio_url(self, location: Location) -> str:
        return "file://{}".format(urllib.parse.quote(self.get_stdio_name(location)))

    def get_stdio_name(self, location: Location) -> str:
        if not location.has_payload():
            return location.get_path()
        else:
            if location._payloads[-1].path:
                parent = location.parent()
                outdir = self._archive_manager.get_extractor_content_dir(parent)
                return os.path.join(outdir, location._payloads[-1].path)
            else:
                outdir = self._archive_manager.get_extractor_content_dir(location)
                return outdir


# EOF #
