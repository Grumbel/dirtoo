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


from typing import Any, Sequence

import collections


def unique(lst: Sequence[object]) -> Sequence[object]:
    """Remove duplicate elements from a list. List can be unsorted."""
    return list(collections.OrderedDict.fromkeys(lst))


def remove_duplicates(lst: Sequence[Any]) -> Sequence[Any]:
    """Remove duplicate elements from a list if they follow on each
    other."""
    result = []
    for idx, x in enumerate(lst):
        if idx != 0 and lst[idx - 1] == x:
            continue
        result.append(x)
    return result


# EOF #
