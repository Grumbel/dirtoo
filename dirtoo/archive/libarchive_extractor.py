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
import libarchive
import threading

from PyQt5.QtCore import pyqtSignal

from dirtoo.archive.extractor import Extractor, ExtractorResult

logger = logging.getLogger(__name__)


def sanitize(pathname: str) -> Optional[str]:
    normpath = os.path.normpath(pathname)
    if os.path.isabs(normpath):
        return None
    elif os.path.split(normpath)[0] == "..":
        return None
    else:
        return normpath


class LibArchiveExtractor(Extractor):

    sig_entry_extracted = pyqtSignal(str, str)

    def __init__(self, filename: str, outdir: str) -> None:
        super().__init__()
        self.filename = os.path.abspath(filename)
        self._outdir = outdir
        self._interruption_event = threading.Event()

    def interrupt(self) -> None:
        self._interruption_event.set()

    def interruption_point(self) -> None:
        if self._interruption_event.is_set():
            raise Exception("{}: archive extraction was interrupted".format(self.filename))

    def extract(self) -> ExtractorResult:
        try:
            self._extract(self._outdir)
            return ExtractorResult.success()
        except Exception as err:
            msg = "{}: failure when extracting archive: {}".format(self.filename, err)
            logger.exception(msg)
            return ExtractorResult.failure(msg)

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


# EOF #
