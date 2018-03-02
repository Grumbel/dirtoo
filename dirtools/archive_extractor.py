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


from typing import List, Tuple

import json
import logging
import os

from PyQt5.QtCore import QObject, QThread, Qt, pyqtSignal

from dirtools.rar_extractor_worker import RarExtractorWorker
from dirtools.sevenzip_extractor_worker import SevenZipExtractorWorker
from dirtools.libarchive_extractor_worker import LibArchiveExtractorWorker
from dirtools.extractor_worker import ExtractorResult

logger = logging.getLogger(__name__)


class ArchiveExtractor(QObject):

    sig_close_requested = pyqtSignal()

    def __init__(self, archive_path: str, outdir: str) -> None:
        super().__init__()

        self._archive_path = archive_path
        self._outdir = outdir
        self._extracted_entries: List[Tuple[str, str]] = []

        contentdir = os.path.join(outdir, "contents")

        # Creating the directory here so that the directory is ready
        # and can be watched, otherwise the thread might not create
        # them in time and a watcher would error out
        if not os.path.isdir(outdir):
            os.makedirs(outdir)

        if not os.path.isdir(contentdir):
            os.makedirs(contentdir)

        if archive_path.lower().endswith(".rar"):
            self._worker = RarExtractorWorker(archive_path, contentdir)
        elif True:
            self._worker = SevenZipExtractorWorker(archive_path, contentdir)
        else:
            self._worker = LibArchiveExtractorWorker(archive_path, contentdir)

        self._thread = QThread(self)
        self._worker.moveToThread(self._thread)

        self._thread.started.connect(self._worker.init)

        self._worker.sig_entry_extracted.connect(self._on_entry_extracted)
        self._worker.sig_finished.connect(self._on_finished)

        # close() is a blocking connection so the thread is properly
        # done after the signal was emit'ed and we don't have to fuss
        # around with sig_finished() and other stuff
        self.sig_close_requested.connect(self._worker.close, type=Qt.BlockingQueuedConnection)

    def _on_started(self):
        js = {
            "status": ExtractorResult.WORKING,
            "message": "Extraction in progress",
            "entries": []
        }
        status_path = os.path.join(self._outdir, "status.json")
        with open(status_path, "w") as fout:
            json.dump(js, fout)
            fout.write("\n")

    def _on_finished(self, result: ExtractorResult):
        js = {
            "status": result.status,
            "message": result.message,
            "entries": self._extracted_entries,
        }
        status_path = os.path.join(self._outdir, "status.json")
        with open(status_path, "w") as fout:
            json.dump(js, fout)
            fout.write("\n")

    def _on_entry_extracted(self, entry: str, outfile: str):
        self._extracted_entries.append((entry, outfile))

    def start(self) -> None:
        self._on_started()
        self._thread.start()

    def close(self) -> None:
        assert self._worker._close is False
        self._worker._close = True
        self.sig_close_requested.emit()
        self._thread.quit()
        self._thread.wait()

    @property
    def sig_entry_extracted(self):
        return self._worker.sig_entry_extracted

    @property
    def sig_finished(self):
        return self._worker.sig_finished


# EOF #
