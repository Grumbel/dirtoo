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

from enum import Enum
import logging
import os
import re

from PyQt5.QtCore import QProcess, QByteArray, pyqtSignal

from dirtools.extractor_worker import ExtractorResult
from dirtools.fileview.worker_thread import Worker

logger = logging.getLogger(__name__)


# FIXME: This won't work with spaces at the end of filenames
BEGIN_RX = re.compile("^Extracting from.*$")
DIR_RX = re.compile("^Creating    ([^\x08]+)[\x080-9% ]*  OK$")
FILE_RX = re.compile("^Extracting  ([^\x08]+)[\x080-9% ]*  OK $")


class State(Enum):

    HEADER = 0
    FILELIST = 1
    RESULT = 2


class RarExtractorWorker(Worker):

    sig_entry_extracted = pyqtSignal(str, str)
    sig_finished = pyqtSignal(ExtractorResult)

    def __init__(self, filename: str, outdir: str) -> None:
        super().__init__()

        self._filename = os.path.abspath(filename)
        self._outdir = outdir

        self._process: Optional[QProcess] = None
        self._errors: List[str] = []

        self._output_state = State.HEADER

    def close(self):
        if self._process is not None:
            self._process.terminate()
            # self._process.kill()

    def on_thread_started(self) -> None:
        try:
            self._start_extract(self._outdir)
        except Exception as err:
            message = "{}: failure when extracting archive".format(self._filename)
            logger.exception(message)
            self.sig_finished.emit(ExtractorResult.failure(message))

    def _start_extract(self, outdir: str) -> None:
        # The directory is already created in ArchiveExtractor
        # os.mkdir(outdir)
        assert os.path.isdir(outdir)

        program = "rar"
        argv = ["x", "-p-", "-c-", self._filename]
        # "-w" + outdir has no effect

        logger.debug("RarExtractorWorker: launching %s %s", program, argv)
        self._process = QProcess()
        self._process.setProgram(program)
        self._process.setArguments(argv)
        self._process.setWorkingDirectory(outdir)
        self._process.start()
        self._process.closeWriteChannel()

        self._process.readyReadStandardOutput.connect(self._on_ready_read_stdout)
        self._process.readyReadStandardError.connect(self._on_ready_read_stderr)
        self._process.errorOccurred.connect(self._on_error_occured)
        self._process.finished.connect(self._on_finished)

    def _on_finished(self, exit_code, exit_status):
        self._process.setCurrentReadChannel(QProcess.StandardOutput)
        for line in os.fsdecode(self._process.readAll().data()).splitlines():
            self._process_stdout(line)

        self._process.setCurrentReadChannel(QProcess.StandardError)
        for line in os.fsdecode(self._process.readAll().data()).splitlines():
            self._process_stderr(line)

        if self._errors != []:
            message = "RAR: " + "\n".join(self._errors)
        else:
            message = ""

        if exit_status != QProcess.NormalExit or exit_code != 0:
            logger.error("RarExtractorWorker: something went wrong: %s  %s", exit_code, exit_status)
            self.sig_finished.emit(ExtractorResult.failure(message))
        else:
            logger.debug("RarExtractorWorker: finished successfully: %s  %s", exit_code, exit_status)
            self.sig_finished.emit(ExtractorResult.success(message))

    def _on_error_occured(self, error) -> None:
        logger.error("RarExtractorWorker: an error occured: %s", error)
        self.sig_finished.emit(ExtractorResult.failure("RarExtractorWorker: an error occured: {}".format(error)))

    def _process_stdout(self, line):
        # print("stdout:", repr(line))

        if self._output_state == State.HEADER:
            if BEGIN_RX.match(line):
                self._output_state = State.FILELIST
        elif self._output_state == State.FILELIST:
            m = FILE_RX.match(line)
            if m:
                entry = m.group(1).rstrip(" ")
                self.sig_entry_extracted.emit(entry, os.path.join(self._outdir, entry))
            else:
                m = DIR_RX.match(line)
                if m:
                    entry = m.group(1).rstrip(" ")
                    self.sig_entry_extracted.emit(entry, os.path.join(self._outdir, entry))
                elif line == "":
                    pass  # ignore empty line at the start
                else:
                    # self._errors.append(line)
                    # self._output_state = State.RESULT
                    pass
        else:
            # self._errors.append(line)
            pass

    def _process_stderr(self, line):
        # print("stderr:", repr(line))
        if line:
            self._errors.append(line)

    def _on_ready_read_stdout(self) -> None:
        self._process.setCurrentReadChannel(QProcess.StandardOutput)
        while self._process.canReadLine():
            buf: QByteArray = self._process.readLine()
            line = os.fsdecode(buf.data())
            line = line.rstrip("\n")
            self._process_stdout(line)

    def _on_ready_read_stderr(self) -> None:
        self._process.setCurrentReadChannel(QProcess.StandardError)
        while self._process.canReadLine():
            buf: QByteArray = self._process.readLine()
            line = os.fsdecode(buf.data())
            line = line.rstrip("\n")
            self._process_stderr(line)


# EOF #
