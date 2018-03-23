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

from PyQt5.QtCore import Qt

logger = logging.getLogger(__name__)


class Filesystem:
    """Low level filesystem operations, mostly just a wrapper around 'os'
    functions with a switch to disable the actual operation and print
    debug information.
    """

    def __init__(self):
        self._enabled = False

    def set_enabled(self, value):
        self._enabled = value

        if self._enabled:
            print("enabling filesystem write access")
        else:
            print("disabling filesystem write access")

    def _message(self, text: str) -> None:
        if self._enabled:
            logger.info(text)
        else:
            print(text)

    def rename(self, oldpath: str, newpath: str) -> None:
        self._message("Filesystem.rename {!r} {!r}".format(oldpath, newpath))

        if self._enabled:
            os.rename(oldpath, newpath)

    def move_files(self, sources: str, destination: str) -> None:
        self._message("Filesystem.move_files {!r} {!r}".format(sources, destination))

        if self._enabled:
            pass

    def copy_files(self, sources: str, destination: str) -> None:
        self._message("Filesystem.copy_files {!r} {!r}".format(sources, destination))

        if self._enabled:
            pass

    def link_files(self, sources: str, destination: str) -> None:
        self._message("Filesystem.link_files {!r} {!r}".format(sources, destination))

        if self._enabled:
            pass

    def do_files(self, action, sources: str, destination: str) -> None:
        if action == Qt.CopyAction:
            self.copy_files(sources, destination)
        elif action == Qt.MoveAction:
            self.move_files(sources, destination)
        elif action == Qt.LinkAction:
            self.link_files(sources, destination)
        else:
            print("unsupported drop action", action)

    def create_directory(self, path: str) -> None:
        self._message("create_directory {!r}".format(path))

        if self._enabled:
            os.mkdir(path)

    def create_file(self, path: str) -> None:
        self._message("create_file:{!r}".format(path))

        if self._enabled:
            with open(path, "xb"):
                pass


# EOF #
