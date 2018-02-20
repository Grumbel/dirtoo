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
import libarchive

from PyQt5.QtCore import QObject, QThread, Qt, pyqtSignal

logger = logging.getLogger(__name__)


def sanitize(pathname):
    normpath = os.path.normpath(pathname)
    if os.path.isabs(normpath):
        return None
    elif os.path.split(normpath)[0] == "..":
        return None
    else:
        return normpath


class ArchiveExtractorWorker(QObject):

    sig_entry_extracted = pyqtSignal(str, str)
    sig_finished = pyqtSignal()

    def __init__(self, filename: str, outdir: str) -> None:
        super().__init__()
        self.filename = os.path.abspath(filename)
        self.outdir = outdir
        self._close = False

    def close(self):
        self._close = True

    def interruption_point(self):
        if self._close:
            raise Exception("{}: archive extraction was interrupted".format(self.filename))

    def init(self) -> None:
        try:
            self._extract(self.outdir)
        except Exception as err:
            logger.exception("{}: failure when extracting archive".format(self.filename))

        self.sig_finished.emit()

    def _extract(self, outdir: str) -> None:
        with libarchive.file_reader(self.filename) as entries:
            self.interruption_point()

            if not os.path.isdir(outdir):
                os.makedirs(outdir)

            for entry in entries:
                pathname = sanitize(entry.pathname)
                if pathname is None:
                    logger.error("skipping unsanitary entry: %s", entry.pathname)
                    continue

                # FIXME: entry.isdir doesn't look reliable, needs more testing
                if entry.isdir or entry.pathname[-1] == '/':
                    dirname = os.path.join(outdir, pathname)
                    if not os.path.isdir(dirname):
                        os.makedirs(dirname)
                    self.sig_entry_extracted.emit(entry.pathname, dirname)
                elif entry.isreg:
                    dirname = os.path.dirname(pathname)

                    if dirname:
                        dirname = os.path.join(outdir, dirname)
                        if not os.path.isdir(dirname):
                            os.makedirs(dirname)

                    while True:
                        outfile = os.path.join(outdir, pathname)
                        with open(outfile, "wb") as out:
                            blocks = entry.get_blocks()
                            for block in blocks:
                                self.interruption_point()
                                out.write(block)
                        self.sig_entry_extracted.emit(entry.pathname, outfile)
                        break
                else:
                    logger.warning("skipping non regular entry: %s", entry.pathname)


class ArchiveExtractor(QObject):

    sig_close_requested = pyqtSignal()

    def __init__(self, filename: str, outdir: str) -> None:
        super().__init__()

        # Creating the directory here so that the directory is ready
        # and can be watched, otherwise the thread might not create
        # them in time and a watcher would error out
        if not os.path.isdir(outdir):
            os.makedirs(outdir)

        self.worker = ArchiveExtractorWorker(filename, outdir)
        self.thread = QThread(self)
        self.worker.moveToThread(self.thread)

        self.thread.started.connect(self.worker.init)

        # close() is a blocking connection so the thread is properly
        # done after the signal was emit'ed and we don't have to fuss
        # around with sig_finished() and other stuff
        self.sig_close_requested.connect(self.worker.close, type=Qt.BlockingQueuedConnection)

    def start(self) -> None:
        self.thread.start()

    def close(self) -> None:
        assert self.worker._close is False
        self.worker._close = True
        self.sig_close_requested.emit()
        self.thread.quit()
        self.thread.wait()

    @property
    def sig_entry_extracted(self):
        return self.worker.sig_entry_extracted

    @property
    def sig_finished(self):
        return self.worker.sig_finished


# EOF #
