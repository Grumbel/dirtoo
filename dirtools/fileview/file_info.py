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


import os


class FileInfo:

    def __init__(self, filename: str) -> None:
        self._filename = filename
        self._abspath = os.path.abspath(self._filename)
        self._dirname = os.path.dirname(self._abspath)
        self._basename = os.path.basename(self._abspath)
        self._isdir = os.path.isdir(self._filename)
        self._stat = os.lstat(self._filename)
        self._ext = os.path.splitext(self._filename)[1]
        self._have_access = os.access(self._filename, os.R_OK)

    @property
    def have_access(self):
        return self._have_access

    @property
    def filename(self):
        return self._filename

    @property
    def abspath(self):
        return self._abspath

    @property
    def dirname(self):
        return self._dirname

    @property
    def basename(self):
        return self._basename

    @property
    def isdir(self):
        return self._isdir

    @property
    def stat(self):
        return self._stat

    @property
    def ext(self):
        return self._ext


# EOF #
