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


class ExtractorStatus:

    @staticmethod
    def from_file(filename: str) -> 'ExtractorStatus':
        try:
            with open(filename, "r") as fin:
                js = json.load(fin)
                return ExtractorStatus(js)
        except Exception as err:
            return ExtractorStatus({
                'status': ExtractorResult.FAILURE,
                'message': "{}: exception while loading: {}".format(filename, err)
            })

    @staticmethod
    def finished(extractor: ArchiveExtractor) -> 'ExtractorStatus':
        js = {
            "path": extractor.get_archive_path(),
            "mtime": extractor.get_mtime(),
            "status": extractor.get_result().status,
            "message": extractor.get_result().message,
            "entries": extractor.get_entries(),
        }
        return ExtractorStatus(js)

    @staticmethod
    def working(extractor: ArchiveExtractor) -> 'ExtractorStatus':
        js = {
            "path": extractor.get_archive_path(),
            "mtime": extractor.get_mtime(),
            "status": ExtractorResult.WORKING,
            "message": "Extraction in progress",
            "entries": []
        }
        return ExtractorStatus(js)

    def __init__(self, js: Dict) -> None:
        self._js = js

    def status(self) -> ExtractorResult:
        return self._js.get("status", ExtractorResult.FAILURE)

    def message(self) -> str:
        return self._js.get("message", "<no message>")

    def save(self, filename: str) -> None:
        with open(filename, "w") as fout:
            json.dump(self._js, fout)
            fout.write("\n")


class ArchiveManager:

    def __init__(self, directory: str) -> None:
        self._extractor_dir = directory
        self._extractors: Dict[Location, ArchiveExtractor] = {}

        if not os.path.isdir(self._extractor_dir):
            os.makedirs(self._extractor_dir)

    def close(self) -> None:
        for location, extractor in self._extractors.items():
            extractor.close()

    def extract(self, location: Location, directory_watcher, stdio) -> None:
        if location in self._extractors:
            return  # extraction already running

        outdir = self._make_extractor_outdir(location)

        if not os.path.isdir(outdir):
            extractor = ArchiveExtractor(stdio.get_stdio_name(location.origin()), outdir)
            self._extractors[location] = extractor
            extractor.sig_started.connect(lambda extractor:
                                          self._on_archive_extractor_started(extractor))
            extractor.sig_finished.connect(lambda extractor, d=directory_watcher:
                                           self._on_archive_extractor_finished(extractor, d))
            extractor.start()
        else:
            status_file = os.path.join(outdir, "status.json")
            status = ExtractorStatus.from_file(status_file)

            if status.status() == ExtractorResult.FAILURE:
                def send_message(dw=directory_watcher, message=status.message()):
                    dw.sig_message.emit(status.message())

                # FIXME: this is a bit of a crude hack to
                # communicate the message to the user
                QTimer.singleShot(0, send_message)
                logging.error("%s: archive exist, but is broken: %s", location, status.message())

    def get_extractor_content_dir(self, location: Location) -> str:
        return os.path.join(self._make_extractor_outdir(location), "contents")

    def _make_extractor_outdir(self, location: Location) -> str:
        assert location._payloads[-1].protocol == "archive"

        origin = location.origin()
        origin._payloads.append(Payload("archive", ""))

        loc_hash = hashlib.md5(origin.as_url().encode()).hexdigest()
        outdir = os.path.join(self._extractor_dir, loc_hash)
        return outdir

    def _on_archive_extractor_started(self, extractor: ArchiveExtractor):
        status = ExtractorStatus.working(extractor)
        status.save(extractor.get_status_file())

    def _on_archive_extractor_finished(self, extractor: ArchiveExtractor,
                                       directory_watcher: DirectoryWatcher) -> None:
        logger.info("ArchiveManager_.on_archive_extractor_finished")
        extractor.close()
        self._extractors = {k: v for k, v in self._extractors.items() if v != extractor}

        result = extractor.get_result()
        if result.status != 0:
            directory_watcher.sig_message.emit(result.message)

        status = ExtractorStatus.finished(extractor)
        status.save(extractor.get_status_file())


# EOF #
