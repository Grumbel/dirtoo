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


from typing import Dict

import os
import logging
import hashlib

from dirtools.fileview.location import Location
from dirtools.fileview.directory_watcher import DirectoryWatcher
from dirtools.archive_extractor import ArchiveExtractor
from dirtools.fileview.file_info import FileInfo

logger = logging.getLogger(__name__)


class VirtualFilesystem:

    def __init__(self, cachedir: str) -> None:
        self.cachedir = cachedir
        self.extractor_dir = os.path.join(self.cachedir, "extractors")

        self.extractors: Dict[Location, ArchiveExtractor] = {}

        if not os.path.isdir(self.extractor_dir):
            os.makedirs(self.extractor_dir)

    def close(self) -> None:
        for location, extractor in self.extractors.items():
            extractor.close()

    def opendir(self, location: Location) -> 'DirectoryWatcher':
        """Create a watcher object for the given directory. The caller is
        responsible for .close()`ing it."""

        if location.has_stdio_name():
            directory_watcher = DirectoryWatcher(self, location)
            return directory_watcher
        else:
            assert len(location.payloads) == 1

            outdir = self._make_extractor_outdir(location)

            if location not in self.extractors:
                extractor = ArchiveExtractor(location.path, outdir)
                self.extractors[location] = extractor
                extractor.sig_finished.connect(lambda x=extractor: self._on_archive_extractor_finished(x))
                extractor.start()

            if location.payloads[0][1]:
                directory_watcher = DirectoryWatcher(self, location, os.path.join(outdir, location.payloads[0][1]))
                return directory_watcher
            else:
                directory_watcher = DirectoryWatcher(self, location, outdir)
                return directory_watcher

    def get_fileinfo(self, location: Location):
        if location.has_stdio_name():
            return FileInfo.from_location(location)
        else:
            assert len(location.payloads) == 1
            outdir = self._make_extractor_outdir(location)
            path = os.path.join(outdir, location.payloads[0][1])
            fi = FileInfo.from_filename(path)
            fi._location = location
            return fi

    def get_stdio_name(self, location):
        if location.has_stdio_name():
            return location.get_stdio_name()
        else:
            assert len(location.payloads) == 1

            outdir = self._make_extractor_outdir(location)
            return os.path.join(outdir, location.payloads[0][1])

    def _make_extractor_outdir(self, location: Location) -> str:
        assert location.payloads[0][0] == "archive"

        loc_hash = hashlib.md5(location.path.encode()).hexdigest()
        return os.path.join(self.extractor_dir, loc_hash)

    def _on_archive_extractor_finished(self, extractor: ArchiveExtractor):
        logger.info("VirtualFilesystem.on_archive_extractor_finished")
        extractor.close()
        self.extractors = {k: v for k, v in self.extractors.items() if v != extractor}

    def get_extractors(self) -> Dict[Location, ArchiveExtractor]:
        return self.extractors


# EOF #
