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


from typing import List, Optional, Iterator

import logging
import subprocess
import fcntl
import os

from PyQt5.QtCore import QObject, QSocketNotifier, pyqtSignal

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


def non_blocking_readline(fp, linesep):
    flag = fcntl.fcntl(fp, fcntl.F_GETFL)
    fcntl.fcntl(fp.fileno(), fcntl.F_SETFL, flag | os.O_NONBLOCK)

    rest = ""
    while True:
        try:
            # The buffer size is chosen to be artificially tiny to not
            # make the GUI unresponsive.
            data = fp.read(16)
        except BlockingIOError:
            yield None
        else:
            if data == "":
                return
            else:
                data = rest + data
                idx = data.find(linesep)
                if idx != -1:
                    rest = data[idx + 1:]
                    yield data[0:idx]
                else:
                    rest = data
                    yield None


class SevenZipExtractorWorker(QObject):

    sig_entry_extracted = pyqtSignal(str, str)
    sig_finished = pyqtSignal()

    def __init__(self, filename: str, outdir: str) -> None:
        super().__init__()

        self._close = False
        self.filename = os.path.abspath(filename)
        self.outdir = outdir

        self.stdout_notifier: Optional[QSocketNotifier] = None
        self.stderr_notifier: Optional[QSocketNotifier] = None

        self.stdout_readliner: Optional[Iterator] = None
        self.stderr_readliner: Optional[Iterator] = None

        self.errors: List[str] = []

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
        proc = subprocess.Popen(
            ["7z", "x", "-ba", "-bb1", "-bd", "-aos",
             "-o" + outdir, self.filename],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)

        print(proc.stdout)
        print(proc.stderr)

        self.stdout_notifier = QSocketNotifier(proc.stdout.fd, QSocketNotifier.Read)
        self.stderr_notifier = QSocketNotifier(proc.stderr.fd, QSocketNotifier.Read)

        self.stdout_readliner = non_blocking_readline(proc.stdout.fd, "\n")
        self.stderr_readliner = non_blocking_readline(proc.stderr.fd, "\n")

        self.stdout_notifier.activated.connect(self._on_stdout_activated)
        self.stderr_notifier.activated.connect(self._on_stderr_activated)

        retval = proc.wait()
        if retval != 0:
            logger.error("SevenZipExtractorWorker: something went wrong: %s", self.errors)

        self.sig_finished()

    def _on_stdout_activated(self, fd: int) -> None:
        try:
            line: str = next(self.stdout_readliner)
        except StopIteration:
            return
        else:
            if line.startswith("- "):
                entry = line[2:]
                self.sig_entry_extracted(entry, os.path.join(self.outdir, entry))

    def _on_stderr_activated(self, fd: int) -> None:
        try:
            line: str = next(self.stderr_readliner)
            self.errors.append(line)
        except StopIteration:
            return


# EOF #
