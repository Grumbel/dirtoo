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

import os

from xdg.DesktopEntry import DesktopEntry


def get_desktop_file(desktop_file: str) -> Optional[str]:
    from xdg.BaseDirectory import xdg_data_dirs

    if not desktop_file:
        return None

    for directory in xdg_data_dirs:
        path = os.path.join(directory, "applications", desktop_file)
        if os.path.exists(path):
            return path

    return None


def get_desktop_entry(desktop_file: str) -> Optional[DesktopEntry]:
    if os.path.isabs(desktop_file):
        return DesktopEntry(desktop_file)
    else:
        filename = get_desktop_file(desktop_file)
        if filename is not None:
            return DesktopEntry(filename)
        else:
            return None


# EOF
