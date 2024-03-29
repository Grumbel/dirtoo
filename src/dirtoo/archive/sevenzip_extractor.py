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

import logging
import os
import traceback

from PyQt6.QtCore import QProcess

from dirtoo.archive.extractor import Extractor, ExtractorResult

logger = logging.getLogger(__name__)


# Get list of files:
# 7zr l -ba -slt ${archive}
# 7z x -ba -aoa -bb1 -bsp0
# -o: output directory
# -ba: hide copyright header
# -bb1: output verbosity level
# -bsp0: redirect progress indicator to /dev/null
# -aoa: overwrite always
# -aou: overwrite rename
# -aot: overwrite rename
# -aos: overwrite skip
# https://sevenzip.osdn.jp/chm/cmdline/switches/index.htm

# Extract file to stdout:
# 7zr x -so ${archive}

# Extract to outdir:
# 7zr x -o outdir ${archive}


class SevenZipExtractor(Extractor):

    def __init__(self, filename: str, outdir: str) -> None:
        super().__init__()

        self._sevenzip = os.environ.get("DIRTOO_7ZIP") or "7zz"

        self._filename = os.path.abspath(filename)
        self._outdir = outdir

        self._process: Optional[QProcess] = None
        self._errors: list[str] = []

        self._error_summary = False
        self._result: Optional[ExtractorResult] = None

    def interrupt(self) -> None:
        if self._process is not None:
            self._process.terminate()
            # self._process.waitForBytesWritten(int msecs = 30000)
            # self._process.kill()

    def extract(self) -> ExtractorResult:
        assert self._process is None

        try:
            self._start_extract(self._outdir)
            assert self._process is not None
            self._process.waitForFinished(-1)  # type: ignore
            assert self._result is not None
            return self._result
        except Exception as err:
            message = "{}: failure when extracting archive: {}".format(self._filename, err)
            logger.exception(message)
            message += "\n\n" + traceback.format_exc()
            return ExtractorResult.failure(message)

    def _start_extract(self, outdir: str) -> None:
        argv = ["x", "-ba", "-bb1", "-bd", "-aos", "-o" + outdir, self._filename]

        logger.debug("SevenZipExtractorWorker: launching %s %s", self._sevenzip, argv)
        self._process = QProcess()
        self._process.setProgram(self._sevenzip)
        self._process.setArguments(argv)

        self._process.readyReadStandardOutput.connect(self._on_ready_read_stdout)
        self._process.readyReadStandardError.connect(self._on_ready_read_stderr)
        self._process.finished.connect(self._on_process_finished)

        self._process.start()
        self._process.closeWriteChannel()

    def _on_process_finished(self, exit_code: int, exit_status: QProcess.ExitStatus) -> None:
        assert self._process is not None

        self._process.setReadChannel(QProcess.ProcessChannel.StandardOutput)
        for line in os.fsdecode(self._process.readAll().data()).splitlines():
            self._process_stdout(line)

        self._process.setReadChannel(QProcess.ProcessChannel.StandardError)
        for line in os.fsdecode(self._process.readAll().data()).splitlines():
            self._process_stderr(line)

        if self._errors != []:
            message = "7-Zip: " + "\n".join(self._errors)
        else:
            message = ""

        if message:
            logger.error("SevenZipExtractorWorker: errors: %s", message)

        if exit_status != QProcess.ExitStatus.NormalExit or exit_code != 0:
            logger.error("SevenZipExtractorWorker: something went wrong: %s  %s", exit_code, exit_status)
            self._result = ExtractorResult.failure(message)
        else:
            logger.debug("SevenZipExtractorWorker: finished successfully: %s  %s", exit_code, exit_status)
            self._result = ExtractorResult.success()

    def _process_stdout(self, line: str) -> None:
        if line.startswith("- "):
            entry = line[2:]

            # 7zz adds a "/" on folders, older versions do not, strip it for uniformity
            if entry[-1] == "/":
                entry = entry[:-1]

            self.sig_entry_extracted.emit(entry, os.path.join(self._outdir, entry))

    def _process_stderr(self, line: str) -> None:
        if line == "ERRORS:":
            self._error_summary = True
        else:
            if self._error_summary:
                if line != "":
                    self._errors.append(line)

    def _on_ready_read_stdout(self) -> None:
        assert self._process is not None

        while self._process.canReadLine():
            buf: bytes = self._process.readLine().data()
            line = os.fsdecode(buf).rstrip("\n")
            # print("stdout:", repr(line))
            self._process_stdout(line)

    def _on_ready_read_stderr(self) -> None:
        assert self._process is not None

        while self._process.canReadLine():
            # print("stderr:", repr(line))
            buf: bytes = self._process.readLine().data()
            line = os.fsdecode(buf).rstrip("\n")
            self._process_stderr(line)


# EOF #
