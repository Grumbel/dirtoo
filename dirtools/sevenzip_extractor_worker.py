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

from PyQt5.QtCore import QObject, QProcess, pyqtSignal

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


class SevenZipExtractorWorker(QObject):

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
            self._extract(self.outdir)
        except Exception as err:
            logger.exception("{}: failure when extracting archive".format(self.filename))

        self.sig_finished.emit()

    def _extract(self, outdir: str) -> None:
        program = "7z"
        argv = ["x", "-ba", "-bb1", "-bd", "-aos", "-o" + outdir, self.filename]

        logger.debug("SevenZipExtractorWorker: launching %s %s", program, argv)
        self.process = QProcess()
        self.process.setProgram(program)
        self.process.setArguments(argv)
        self.process.start()

        self.process.readyReadStandardOutput.connect(self._on_ready_read_stdout)
        self.process.readyReadStandardError.connect(self._on_ready_read_stderr)

        self.process.finished.connect(self._on_finished)

    def _on_finished(self, exit_code, exit_status):
        if exit_status != QProcess.NormalExit:
            logger.error("SevenZipExtractorWorker: something went wrong: %s  %s", exit_code, exit_status)
        else:
            logger.debug("SevenZipExtractorWorker: finished successfully: %s  %s", exit_code, exit_status)

        if self.errors != []:
            logger.error("SevenZipExtractorWorker: errors: %s  %s", "\n".join(self.errors))

        self.sig_finished.emit()

    def _on_ready_read_stdout(self) -> None:
        while self.process.canReadLine():
            buf = self.process.readLine()
            line = os.fsdecode(buf.data()).rstrip("\n")
            # print("stdout:", repr(line))

            if line.startswith("- "):
                entry = line[2:]
                self.sig_entry_extracted.emit(entry, os.path.join(self.outdir, entry))

    def _on_ready_read_stderr(self) -> None:
        while self.process.canReadLine():
            # print("stderr:", repr(line))
            buf = self.process.readLine()
            line = os.fsdecode(buf.data()).rstrip("\n")
            self.errors.append(line)


# EOF #
