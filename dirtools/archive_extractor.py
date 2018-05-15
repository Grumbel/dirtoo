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


from typing import List, Tuple, Optional

import json
import logging
import os

from PyQt5.QtCore import Qt, pyqtSignal

from dirtools.rar_extractor import RarExtractor
from dirtools.sevenzip_extractor import SevenZipExtractor
from dirtools.libarchive_extractor import LibArchiveExtractor
from dirtools.extractor import Extractor, ExtractorResult
from dirtools.fileview.worker_thread import WorkerThread, Worker

logger = logging.getLogger(__name__)


class ArchiveExtractorWorker(Worker):

    sig_entry_extracted = pyqtSignal(str, str)
    sig_finished = pyqtSignal(ExtractorResult)

    def __init__(self, archive_path: str, contentdir: str) -> None:
        super().__init__()
        self._archive_path = archive_path
        self._contentdir = contentdir
        self._extractor: Optional[Extractor] = None

    def on_thread_started(self) -> None:
        # FIXME: Use mime-type to decide proper extractor
        if self._archive_path.lower().endswith(".rar"):
            extractor = RarExtractor(self._archive_path, self._contentdir)
        elif True:  # pylint: disable=using-constant-test
            extractor = SevenZipExtractor(self._archive_path, self._contentdir)
        else:
            extractor = LibArchiveExtractor(self._archive_path, self._contentdir)

        # forward signals
        extractor.sig_entry_extracted.connect(self.sig_entry_extracted)
        extractor.sig_finished.connect(self.sig_finished)

        self._extractor = extractor

        extractor.extract()

    def close(self) -> None:
        self._extractor.interrupt()


class ArchiveExtractor(WorkerThread):

    sig_close_requested = pyqtSignal()

    def __init__(self, archive_path: str, outdir: str) -> None:
        super().__init__()

        self._archive_path = archive_path
        self._outdir = outdir
        self._extracted_entries: List[Tuple[str, str]] = []

        stat = os.stat(self._archive_path)
        self._mtime = stat.st_mtime

        contentdir = os.path.join(outdir, "contents")

        # Creating the directory here so that the directory is ready
        # and can be watched, otherwise the thread might not create
        # them in time and a watcher would error out
        if not os.path.isdir(outdir):
            os.makedirs(outdir)

        if not os.path.isdir(contentdir):
            os.makedirs(contentdir)

        worker = ArchiveExtractorWorker(archive_path, contentdir)
        self.set_worker(worker)

        worker.sig_entry_extracted.connect(self._on_entry_extracted)
        worker.sig_finished.connect(self._on_worker_finished)

        # close() is a blocking connection so the thread is properly
        # done after the signal was emit'ed and we don't have to fuss
        # around with sig_finished() and other stuff
        self.sig_close_requested.connect(worker.close, type=Qt.BlockingQueuedConnection)

    def _on_worker_started(self):
        js = {
            "path": self._archive_path,
            "mtime": self._mtime,
            "status": ExtractorResult.WORKING,
            "message": "Extraction in progress",
            "entries": []
        }
        status_path = os.path.join(self._outdir, "status.json")
        with open(status_path, "w") as fout:
            json.dump(js, fout)
            fout.write("\n")

    def _on_worker_finished(self, result: ExtractorResult):
        js = {
            "path": self._archive_path,
            "mtime": self._mtime,
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
        super().start()
        self._on_worker_started()

    @property
    def sig_entry_extracted(self):
        return self._worker.sig_entry_extracted

    @property
    def sig_finished(self):
        return self._worker.sig_finished


# EOF #
