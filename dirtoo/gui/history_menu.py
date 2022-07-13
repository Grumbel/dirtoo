# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2017 Ingo Ruhnke <grumbel@gmail.com>
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


from typing import TYPE_CHECKING, Sequence

from dirtoo.gui.menu import Menu
from dirtoo.image.icon import load_icon

if TYPE_CHECKING:
    from dirtoo.fileview.controller import Controller
    from dirtoo.filesystem.location import Location


def make_history_menu_entries(controller: 'Controller', history_menu: Menu, entries: Sequence['Location']) -> None:
    icon = load_icon("folder")
    for entry in entries:
        action = history_menu.addDoubleAction(
            icon, entry.as_url(),
            lambda entry=entry: controller.set_location(entry),  # type: ignore
            lambda entry=entry: controller.app.show_location(entry))  # type: ignore
        # FIXME: exists() checks must be done asynchronously
        if False and not entry.exists():  # type: ignore  # pylint: disable=R1727
            action.setEnabled(False)  # type: ignore


# EOF #
