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


from typing import List

import os


class PathCompletion:

    def __init__(self) -> None:
        pass

    def complete(self, text) -> List[str]:
        dirname = os.path.dirname(text)
        basename = os.path.basename(text)

        candidates = []

        try:
            for entry in os.scandir(dirname):
                if entry.is_dir():
                    if entry.name.startswith(basename):
                        candidates.append(entry.path)
        except OSError:
            pass

        return candidates


# EOF #
