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


from typing import List, Optional

import logging
import os
import re

from PyQt5.QtCore import QObject, QProcess, pyqtSignal

logger = logging.getLogger(__name__)


# FIXME: This won't work with spaces at the end of filenames
DIR_RX = re.compile("^Creating    ([^\x08]+)[\x080-9% ]*  OK$")
FILE_RX = re.compile("^Extracting  ([^\x08]+)[\x080-9% ]*  OK $")


class RarExtractorWorker(QObject):

    sig_entry_extracted = pyqtSignal(str, str)
    sig_finished = pyqtSignal()

    def __init__(self, filename: str, outdir: str) -> None:
        super().__init__()

        self._close = False
        self.filename = os.path.abspath(filename)
        self.outdir = outdir

        self.process: Optional[QProcess] = None
        self.errors: List[str] = []

    def close(self):
        self._close = True

        if self.process is not None:
            self.process.terminate()
            # self.process.kill()

    def init(self) -> None:
        try:
            self._start_extract(self.outdir)
        except Exception as err:
            logger.exception("{}: failure when extracting archive".format(self.filename))
            self.sig_finished.emit()

    def _start_extract(self, outdir: str) -> None:
        # The directory is already created in ArchiveExtractor
        # os.mkdir(outdir)
        # assert os.path.isdir(outdir)

        program = "rar"
        argv = ["x", "-p-", "-c-", self.filename]
        # "-w" + outdir has no effect

        logger.debug("RarExtractorWorker: launching %s %s", program, argv)
        self.process = QProcess()
        self.process.setProgram(program)
        self.process.setArguments(argv)
        self.process.setWorkingDirectory(outdir)
        self.process.start()
        self.process.closeWriteChannel()

        self.process.readyReadStandardOutput.connect(self._on_ready_read_stdout)
        self.process.readyReadStandardError.connect(self._on_ready_read_stderr)
        self.process.errorOccurred.connect(self._on_error_occured)
        self.process.finished.connect(self._on_finished)

    def _on_finished(self, exit_code, exit_status):
        if exit_status != QProcess.NormalExit:
            logger.error("RarExtractorWorker: something went wrong: %s  %s", exit_code, exit_status)
        else:
            logger.debug("RarExtractorWorker: finished successfully: %s  %s", exit_code, exit_status)

        if self.errors != []:
            logger.error("RarExtractorWorker: errors: %s  %s", "\n".join(self.errors))

        self.sig_finished.emit()

    def _on_ready_read_stdout(self) -> None:
        while self.process.canReadLine():
            buf = self.process.readLine()
            line = os.fsdecode(buf.data()).rstrip("\n")
            # print("stdout:", repr(line))

            m = FILE_RX.match(line)
            if m:
                entry = m.group(1).rstrip()
                self.sig_entry_extracted.emit(entry, os.path.join(self.outdir, entry))
            else:
                m = DIR_RX.match(line)
                if m:
                    entry = m.group(1).rstrip()
                    self.sig_entry_extracted.emit(entry, os.path.join(self.outdir, entry))

    def _on_ready_read_stderr(self) -> None:
        while self.process.canReadLine():
            # print("stderr:", repr(line))
            buf = self.process.readLine()
            line = os.fsdecode(buf.data()).rstrip("\n")
            self.errors.append(line)

    def _on_error_occured(self, error) -> None:
        logger.error("RarExtractorWorker: an error occured: %s", error)
        self.sig_finished.emit()


# EOF #
