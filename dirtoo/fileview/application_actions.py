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


from typing import TYPE_CHECKING

from PyQt5.QtWidgets import QAction

from dirtoo.image.icon import load_icon

if TYPE_CHECKING:
    from dirtoo.fileview.application import FileViewApplication


class ApplicationActions:

    def __init__(self, app: 'FileViewApplication') -> None:
        self._app = app
        self._make_actions()

    def _make_actions(self) -> None:
        self.enable_filesystem = QAction(load_icon("drive-harddisk"),
                                         "Allow file manipulation")
        self.enable_filesystem.setCheckable(True)
        self.enable_filesystem.setChecked(False)
        self.enable_filesystem.triggered.connect(self._app.set_filesystem_enabled)


# EOF #
