# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2022 Ingo Ruhnke <grumbel@gmail.com>
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

from PyQt6.QtGui import QIcon


logger = logging.getLogger(__name__)


def load_icon(name: str) -> QIcon:
    icon = QIcon.fromTheme(name)
    if icon.isNull():
        logger.error(f"could not find icon '{name}`")

    return icon


# EOF #
