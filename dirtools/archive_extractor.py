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


import logging
import os

from PyQt5.QtCore import QObject, QThread, Qt, pyqtSignal

from dirtools.rar_extractor_worker import RarExtractorWorker
from dirtools.sevenzip_extractor_worker import SevenZipExtractorWorker
from dirtools.libarchive_extractor_worker import LibArchiveExtractorWorker

logger = logging.getLogger(__name__)


class ArchiveExtractor(QObject):

    sig_close_requested = pyqtSignal()

    def __init__(self, filename: str, outdir: str) -> None:
        super().__init__()

        # Creating the directory here so that the directory is ready
        # and can be watched, otherwise the thread might not create
        # them in time and a watcher would error out
        if not os.path.isdir(outdir):
            os.makedirs(outdir)

        if filename.lower().endswith(".rar"):
            self._worker = RarExtractorWorker(filename, outdir)
        elif True:
            self._worker = SevenZipExtractorWorker(filename, outdir)
        else:
            self._worker = LibArchiveExtractorWorker(filename, outdir)

        self._thread = QThread(self)
        self._worker.moveToThread(self._thread)

        self._thread.started.connect(self._worker.init)

        # close() is a blocking connection so the thread is properly
        # done after the signal was emit'ed and we don't have to fuss
        # around with sig_finished() and other stuff
        self.sig_close_requested.connect(self._worker.close, type=Qt.BlockingQueuedConnection)

    def start(self) -> None:
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
