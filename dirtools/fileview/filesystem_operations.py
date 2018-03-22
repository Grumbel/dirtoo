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


from typing import Optional

import logging

from PyQt5.QtWidgets import QWidget

from dirtools.fileview.location import Location
from dirtools.fileview.rename_operation import RenameOperation

logger = logging.getLogger(__name__)


class FilesystemOperations:
    """Filesystem operations for the GUI. Messageboxes and dialogs will be
    shown on errors and conflicts."""

    def __init__(self, logfile=None):
        self._logfile = logfile
        self._rename_op = RenameOperation()

    def rename_location(self, location: Location, parent: Optional[QWidget] = None) -> None:
        self._rename_op.rename_location(location, parent)

    def move_files(self, sources: str, destination: str) -> None:
        print("FilesystemOperations.move_files", sources, destination)

    def copy_files(self, sources: str, destination: str) -> None:
        print("FilesystemOperations.copy_files", sources, destination)

    def link_files(self, sources: str, destination: str) -> None:
        print("FilesystemOperations.link_files", sources, destination)


# EOF #
