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

import json
import os
import logging
import hashlib
import urllib

from PyQt5.QtCore import QTimer

from dirtools.extractor_worker import ExtractorResult
from dirtools.fileview.location import Location, Payload
from dirtools.fileview.directory_watcher import DirectoryWatcher
from dirtools.archive_extractor import ArchiveExtractor
from dirtools.fileview.file_info import FileInfo

logger = logging.getLogger(__name__)


class StdioFilesystem:

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

            directory_watcher = DirectoryWatcher(self, location)

            if not os.path.isdir(outdir) and location not in self.extractors:
                extractor = ArchiveExtractor(self.get_stdio_name(location.origin()), outdir)
                self.extractors[location] = extractor
                extractor.sig_finished.connect(lambda result=None, x=extractor, d=directory_watcher:
                                               self._on_archive_extractor_finished(x, d, result))
                extractor.start()
            else:
                status_file = os.path.join(outdir, "status.json")
                with open(status_file, "r") as fin:
                    js = json.load(fin)
                if "status" in js and \
                   js["status"] in [ExtractorResult.SUCCESS, ExtractorResult.WORKING]:
                    pass  # everything ok
                else:
                    if "message" in js:
                        message = js["message"]
                    else:
                        message = "<no message>"

                    def send_message(dw=directory_watcher, message=message):
                        dw.sig_message.emit(message)

                    # FIXME: this is a bit of a crude hack to
                    # communicate the message to the user
                    QTimer.singleShot(0, send_message)
                    logging.error("%s: archive exist, but is broken: %s", location, message)

            return directory_watcher

    def get_fileinfo(self, location: Location) -> FileInfo:
        if not location.has_payload():
            fi = FileInfo.from_filename(location.get_path())
            fi._location = location
            return fi
        else:
            parent = location.parent()
            assert parent.has_payload()

            outdir = os.path.join(self._make_extractor_outdir(parent), "contents")
            path = os.path.join(outdir, location._payloads[-1].path)

            fi = FileInfo.from_filename(path)
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
                outdir = os.path.join(self._make_extractor_outdir(parent), "contents")
                return os.path.join(outdir, location._payloads[-1].path)
            else:
                outdir = os.path.join(self._make_extractor_outdir(location), "contents")
                return outdir

    def _make_extractor_outdir(self, location: Location) -> str:
        assert location._payloads[-1].protocol == "archive"

        origin = location.origin()
        origin._payloads.append(Payload("archive", ""))

        loc_hash = hashlib.md5(origin.as_url().encode()).hexdigest()
        outdir = os.path.join(self.extractor_dir, loc_hash)
        return outdir

    def _on_archive_extractor_finished(self, extractor: ArchiveExtractor,
                                       directory_watcher: DirectoryWatcher,
                                       result=None) -> None:
        logger.info("VirtualFilesystem.on_archive_extractor_finished")
        extractor.close()
        self.extractors = {k: v for k, v in self.extractors.items() if v != extractor}

        if result is not None and result.status != 0:
            directory_watcher.sig_message.emit(result.message)


# EOF #
