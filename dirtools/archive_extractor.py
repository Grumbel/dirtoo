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
import hashlib

from PyQt5.QtCore import QObject, pyqtSignal

logger = logging.getLogger(__name__)


def sanitize(pathname):
    normpath = os.path.normpath(pathname)
    if os.path.isabs(normpath):
        return None
    elif os.path.split(normpath)[0] == "..":
        return None
    else:
        return normpath


class ArchiveExtractor(QObject):

    sig_entry_extracted = pyqtSignal(str, str)

    def __init__(self, filename: str) -> None:
        super().__init__()
        self.filename = os.path.abspath(filename)

    def extract(self, outdir) -> None:
        with libarchive.file_reader(self.filename) as entries:
            for entry in entries:
                pathname = sanitize(entry.pathname)
                if pathname is None:
                    logger.error("skipping unsanitary entry: %s", entry.pathname)
                    continue

                if entry.isreg:
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
                                out.write(block)
                        self.sig_entry_extracted.emit(entry.pathname, outfile)
                        break
                elif entry.isdir:
                    dirname = os.path.join(outdir, pathname)
                    if not os.path.isdir(dirname):
                        os.makedirs(dirname)
                    self.sig_entry_extracted.emit(entry.pathname, dirname)
                else:
                    logger.warning("skipping non regular entry: %s", entry.pathname)


# EOF #
