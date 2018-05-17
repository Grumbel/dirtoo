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

from typing import Dict

import json
import os
import logging
import hashlib

from PyQt5.QtCore import QTimer

from dirtools.extractor import ExtractorResult
from dirtools.fileview.location import Location, Payload
from dirtools.fileview.directory_watcher import DirectoryWatcher
from dirtools.archive_extractor import ArchiveExtractor

logger = logging.getLogger(__name__)


class ArchiveManager:

    def __init__(self, directory: str) -> None:
        self.extractor_dir = directory
        self.extractors: Dict[Location, ArchiveExtractor] = {}

        if not os.path.isdir(self.extractor_dir):
            os.makedirs(self.extractor_dir)

    def close(self) -> None:
        for location, extractor in self.extractors.items():
            extractor.close()

    def extract(self, location: Location, directory_watcher, stdio) -> None:
        outdir = self._make_extractor_outdir(location)

        if not os.path.isdir(outdir) and location not in self.extractors:
            extractor = ArchiveExtractor(stdio.get_stdio_name(location.origin()), outdir)
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

    def get_extractor_content_dir(self, location: Location) -> str:
        return os.path.join(self._make_extractor_outdir(location), "contents")

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
