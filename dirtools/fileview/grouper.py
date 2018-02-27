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


from typing import Any, cast

from functools import total_ordering
from datetime import datetime


@total_ordering
class Group:

    def __init__(self, label: str, value: Any) -> None:
        self.label: str = label
        self.value: Any = value

    def __eq__(self, other: Any) -> bool:
        if isinstance(other, Group):
            return cast(bool, self.value == other.value)
        else:
            return False

    def __ne__(self, other: Any) -> bool:
        return not (self == other)

    def __lt__(self, other: 'Group') -> bool:
        return cast(bool, self.value < other.value)

    def __hash__(self):
        return self.value

    def __str__(self):
        return self.label


class NoGrouperFunc:

    def __init__(self):
        pass

    def __call__(self, fileinfos):
        for fileinfo in fileinfos:
            fileinfo.group = None


class DayGrouperFunc:

    def __init__(self):
        pass

    def __call__(self, fileinfos):
        for fileinfo in fileinfos:
            if fileinfo.isdir():
                fileinfo.group = None
            else:
                date = datetime.fromtimestamp(fileinfo.mtime())
                fileinfo.group = date.isocalendar()


class DirectoryGrouperFunc:

    def __init__(self):
        pass

    def __call__(self, fileinfos):
        for fileinfo in fileinfos:
            fileinfo.group = fileinfo.dirname()


class DurationGrouperFunc:

    def __init__(self):
        self._buckets = [
            (lambda x: x > 60, Group("Very Long >60 minutes", 4)),
            (lambda x: x > 30, Group("Long (< 60 minutes)", 3)),
            (lambda x: x > 10, Group("Medium (< 30 minutes)", 2)),
            (lambda x: x > 5, Group("Short (< 10 minutes)", 1)),
            (lambda x: True, Group("Very Short (< 5 minutes)", 0)),
        ]

    def _find_bucket(self, x):
        for (bucket, group) in self._buckets:
            if bucket(x):
                return group
        return None

    def __call__(self, fileinfos):
        for fileinfo in fileinfos:
            metadata = fileinfo.metadata()
            if "duration" in metadata:
                fileinfo.group = self._find_bucket(metadata["duration"] / 60000)
            else:
                fileinfo.group = None


class Grouper:

    def __init__(self):
        self.grouper_func = NoGrouperFunc()

    def set_func(self, func):
        self.grouper_func = func

    def apply(self, fileinfos):
        if self.grouper_func is None:
            return
        else:
            self.grouper_func(fileinfos)

# EOF #
