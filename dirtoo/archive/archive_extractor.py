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


from typing import Sequence, Tuple, Optional

import logging
import os

from PyQt5.QtCore import pyqtSignal

from dirtoo.archive.extractor import Extractor, ExtractorResult
from dirtoo.archive.extractor_factory import make_extractor
from dirtoo.fileview.worker_thread import WorkerThread, Worker

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
        extractor = make_extractor(self._archive_path, self._contentdir)

        # forward signals
        extractor.sig_entry_extracted.connect(self.sig_entry_extracted.emit)

        self._extractor = extractor

        result = extractor.extract()
        self.sig_finished.emit(result)

    def interrupt(self) -> None:
        assert self._extractor is not None

        self._extractor.interrupt()

    def close(self) -> None:
        assert self._extractor is not None

        self._extractor.interrupt()


class ArchiveExtractor(WorkerThread):

    sig_started = pyqtSignal(object)  # ArchiveExtractor
    sig_finished = pyqtSignal(object)  # ArchiveExtractor

    def __init__(self, archive_path: str, outdir: str) -> None:
        super().__init__()

        self._archive_path = archive_path
        self._outdir = outdir
        self._extracted_entries: list[Tuple[str, str]] = []
        self._result: Optional[ExtractorResult] = None

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

    def get_archive_path(self) -> str:
        return self._archive_path

    def get_mtime(self) -> float:
        return self._mtime

    def get_result(self) -> ExtractorResult:
        assert self._result is not None
        return self._result

    def get_entries(self) -> Sequence[str]:
        return [i[0] for i in self._extracted_entries]

    # FIXME: this doesn't really belong in this class
    def get_status_file(self) -> str:
        return os.path.join(self._outdir, "status.json")

    def _on_entry_extracted(self, entry: str, outfile: str) -> None:
        self._extracted_entries.append((entry, outfile))

    def _on_worker_finished(self, result: ExtractorResult) -> None:
        self._result = result
        self.sig_finished.emit(self)

    def start(self) -> None:
        super().start()
        self.sig_started.emit(self)

    @property
    def sig_entry_extracted(self) -> pyqtSignal:
        assert self._worker is not None
        return self._worker.sig_entry_extracted


# EOF #
