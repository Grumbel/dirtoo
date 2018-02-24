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


from typing import Dict, cast

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

        if not location.has_payload():
            directory_watcher = DirectoryWatcher(self, location)
            return directory_watcher
        else:
            outdir = self._make_extractor_outdir(location)

            if not os.path.isdir(outdir) and location not in self.extractors:
                extractor = ArchiveExtractor(self.get_stdio_name(location.origin()), outdir)
                self.extractors[location] = extractor
                extractor.sig_finished.connect(lambda x=extractor: self._on_archive_extractor_finished(x))
                extractor.start()

            directory_watcher = DirectoryWatcher(self, location)
            return directory_watcher

    def get_fileinfo(self, location: Location) -> FileInfo:
        if not location.has_payload():
            fi = FileInfo.from_filename(location.get_path())
            fi._location = location
            return fi
        else:
            parent = location.parent()
            assert parent.has_payload()

            outdir = self._make_extractor_outdir(parent)
            path = os.path.join(outdir, location._payloads[-1].path)

            fi = FileInfo.from_filename(path)
            fi._location = location
            return fi

    def get_stdio_name(self, location: Location) -> str:
        if not location.has_payload():
            return cast(str, location.get_path())
        else:
            if location._payloads[-1].path:
                parent = location.parent()
                outdir = self._make_extractor_outdir(parent)
                return cast(str, os.path.join(outdir, location._payloads[-1].path))
            else:
                outdir = self._make_extractor_outdir(location)
                return outdir

    def _make_extractor_outdir(self, location: Location) -> str:
        assert location._payloads[-1].protocol == "archive"

        loc_hash = hashlib.md5(location.as_url().encode()).hexdigest()
        return os.path.join(self.extractor_dir, loc_hash)

    def _on_archive_extractor_finished(self, extractor: ArchiveExtractor) -> None:
        logger.info("VirtualFilesystem.on_archive_extractor_finished")
        extractor.close()
        self.extractors = {k: v for k, v in self.extractors.items() if v != extractor}

    def get_extractors(self) -> Dict[Location, ArchiveExtractor]:
        return self.extractors


# EOF #
