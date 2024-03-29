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


from typing import Optional

from enum import Enum
import logging
import os
import re

from PyQt6.QtCore import QProcess

from dirtoo.archive.extractor import Extractor, ExtractorResult


logger = logging.getLogger(__name__)


# FIXME: This won't work with spaces at the end of filenames
BEGIN_RX = re.compile("^Extracting from.*$")
DIR_RX = re.compile("^Creating    ([^\x08]+)[\x080-9% ]*  OK$")
FILE_RX = re.compile("^Extracting  ([^\x08]+)[\x080-9% ]*  OK $")


class State(Enum):

    HEADER = 0
    FILELIST = 1
    RESULT = 2


class RarExtractor(Extractor):

    @staticmethod
    def is_available() -> bool:
        return bool(os.environ.get("DIRTOO_RAR"))

    def __init__(self, filename: str, outdir: str) -> None:
        super().__init__()

        self._rar = os.environ.get("DIRTOO_RAR") or "rar"

        self._filename = os.path.abspath(filename)
        self._outdir = outdir

        self._process: Optional[QProcess] = None
        self._errors: list[str] = []

        self._result: Optional[ExtractorResult] = None

        self._output_state = State.HEADER

    def interrupt(self) -> None:
        if self._process is not None:
            self._process.terminate()
            # self._process.kill()

    def extract(self) -> ExtractorResult:
        try:
            self._start_extract(self._outdir)
            assert self._process is not None
            self._process.waitForFinished(-1)
            assert self._result is not None
            return self._result
        except Exception:
            message = "{}: failure when extracting archive".format(self._filename)
            logger.exception(message)
            return ExtractorResult.failure(message)

    def _start_extract(self, outdir: str) -> None:
        # The directory is already created in ArchiveExtractor
        # os.mkdir(outdir)
        assert os.path.isdir(outdir)

        argv = ["x", "-p-", "-c-", self._filename]
        # "-w" + outdir has no effect

        logger.debug("RarExtractorWorker: launching %s %s", self._rar, argv)
        self._process = QProcess()
        self._process.setProgram(self._rar)
        self._process.setArguments(argv)
        self._process.setWorkingDirectory(outdir)
        self._process.start()
        self._process.closeWriteChannel()

        self._process.readyReadStandardOutput.connect(self._on_ready_read_stdout)
        self._process.readyReadStandardError.connect(self._on_ready_read_stderr)
        self._process.errorOccurred.connect(self._on_error_occured)
        self._process.finished.connect(self._on_process_finished)

    def _on_process_finished(self, exit_code: int, exit_status: QProcess.ExitStatus) -> None:
        assert self._process is not None

        self._process.setReadChannel(QProcess.ProcessChannel.StandardOutput)
        for line in os.fsdecode(self._process.readAll().data()).splitlines():
            self._process_stdout(line)

        self._process.setReadChannel(QProcess.ProcessChannel.StandardError)
        for line in os.fsdecode(self._process.readAll().data()).splitlines():
            self._process_stderr(line)

        if self._errors != []:
            message = "RAR: " + "\n".join(self._errors)
        else:
            message = ""

        if exit_status != QProcess.ExitStatus.NormalExit or exit_code != 0:
            logger.error("RarExtractorWorker: something went wrong: %s  %s", exit_code, exit_status)
            self._result = ExtractorResult.failure(message)
        else:
            logger.debug("RarExtractorWorker: finished successfully: %s  %s", exit_code, exit_status)
            self._result = ExtractorResult.success(message)

    def _on_error_occured(self, error: QProcess.ProcessError) -> None:
        logger.error("RarExtractorWorker: an error occured: %s", error)
        self._result = ExtractorResult.failure("RarExtractorWorker: an error occured: {}".format(error))

    def _process_stdout(self, line: str) -> None:
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

    def _process_stderr(self, line: str) -> None:
        # print("stderr:", repr(line))
        if line:
            self._errors.append(line)

    def _on_ready_read_stdout(self) -> None:
        assert self._process is not None

        self._process.setReadChannel(QProcess.ProcessChannel.StandardOutput)
        while self._process.canReadLine():
            buf: bytes = self._process.readLine().data()
            line = os.fsdecode(buf)
            line = line.rstrip("\n")
            self._process_stdout(line)

    def _on_ready_read_stderr(self) -> None:
        assert self._process is not None

        self._process.setReadChannel(QProcess.ProcessChannel.StandardError)
        while self._process.canReadLine():
            buf: bytes = self._process.readLine().data()
            line = os.fsdecode(buf)
            line = line.rstrip("\n")
            self._process_stderr(line)


# EOF #
