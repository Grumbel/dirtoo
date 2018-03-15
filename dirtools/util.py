# dirtool.py - diff tool for directories
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


from typing import List, Any

import re
import os
import collections


def expand_file(f: str, recursive: bool):
    if os.path.isdir(f):
        if recursive:
            lst = [expand_file(os.path.join(f, x), recursive) for x in os.listdir(f)]
            return [item for sublist in lst for item in sublist]
        else:
            return [os.path.join(f, x) for x in os.listdir(f)]
    else:
        return [f]


def expand_directories(files: List[str], recursive: bool):
    results: List[str] = []
    for f in files:
        results += expand_file(f, recursive)
    return results


NUMERIC_SORT_RX = re.compile(r'(\d+)')


def numeric_sort_key(text):
    return tuple(int(sub) if sub.isdigit() else sub
                 for sub in NUMERIC_SORT_RX.split(text) if sub != "")


def numeric_sorted(lst: List[str]) -> List[str]:
    return sorted(lst, key=numeric_sort_key)


def unique(lst: List[object]) -> List[object]:
    """Remove duplicate elements from a list. List can be unsorted."""
    return list(collections.OrderedDict.fromkeys(lst))


def remove_duplicates(lst: List[Any]) -> List[Any]:
    """Remove duplicate elements from a list if they follow on each
    other."""
    result = []
    for idx, x in enumerate(lst):
        if idx != 0 and lst[idx - 1] == x:
            continue
        result.append(x)
    return result


GLOB_PATTERN_RX = re.compile(r"[\*\?\[\]]")


def is_glob_pattern(text):
    return bool(GLOB_PATTERN_RX.search(text))


# EOF #
