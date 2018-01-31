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


class FileInfo:

    def __init__(self, filename: str) -> None:
        logging.debug("FileInfo.__init__: %s", filename)
        self._filename = filename
        self._abspath = os.path.abspath(self._filename)
        self._dirname = os.path.dirname(self._abspath)
        self._basename = os.path.basename(self._abspath)
        self._isdir = os.path.isdir(self._filename)
        self._stat = os.lstat(self._filename)
        self._ext = os.path.splitext(self._filename)[1]
        self._have_access = os.access(self._filename, os.R_OK)
        self.is_excluded = False
        self.is_hidden = False

    @property
    def is_visible(self):
        return not self.is_hidden and not self.is_excluded

    def have_access(self):
        return self._have_access

    def filename(self):
        return self._filename

    def abspath(self):
        return self._abspath

    def dirname(self):
        return self._dirname

    def basename(self):
        return self._basename

    def isdir(self):
        return self._isdir

    def stat(self):
        return self._stat

    def ext(self):
        return self._ext

    def size(self):
        return self._stat.st_size

    def mtime(self):
        return self._stat.st_mtime

    def __str__(self):
        return "FileInfo({})".format(self._abspath)

# EOF #
