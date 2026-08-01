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

from dirtoo.gui.menu import Menu
from dirtoo.image.icon import load_icon

if TYPE_CHECKING:
    from dirtoo.fileview.controller import Controller
    from dirtoo.filesystem.location import Location


class DirectoryContextMenu(Menu):

    def __init__(self, controller: 'Controller', location: 'Location') -> None:
        super().__init__()

        self._controller = controller
        self._location = location

        self._build_menu()

    def _build_menu(self) -> None:
        self.addAction(load_icon('folder-new'), "Create Directory",
                       self._controller.create_directory)
        self.addAction(load_icon('document-new'), "Create Empty File",
                       self._controller.create_file)
        self.addSeparator()
        self.addAction(self._controller.actions.edit_paste)
        self.addSeparator()

        if self._location is not None:
            self.addAction(load_icon('utilities-terminal'), "Open Terminal Here",
                           lambda location=self._location: self._controller.app.executor.launch_terminal(location))
            self.addSeparator()

        self.addAction(self._controller.actions.edit_select_all)
        self.addSeparator()
        self.addAction(load_icon('document-properties'), "Properties...")


# EOF #
