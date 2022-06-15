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


from typing import TYPE_CHECKING, Any, Dict, cast

from functools import total_ordering
from datetime import datetime

if TYPE_CHECKING:
    from dirtoo.fileview.file_info import FileInfo  # noqa: F401


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
        return hash(self.value)

    def __str__(self):
        return self.label


class Grouper:

    def __init__(self) -> None:
        pass

    def __call__(self, fileinfo: 'FileInfo'):
        pass


class NoGrouper(Grouper):

    def __init__(self) -> None:
        pass

    def __call__(self, fileinfo: 'FileInfo'):
        fileinfo.group = None


class DayGrouper(Grouper):

    def __init__(self) -> None:
        self._groups: Dict[str, Group] = {}

    def __call__(self, fileinfo: 'FileInfo'):
        if fileinfo.isdir():
            fileinfo.group = None
        else:
            date = datetime.fromtimestamp(fileinfo.mtime())
            date_str = date.strftime("%Y-%m-%d")
            group = self._groups.get(date_str, None)
            if group is None:
                group = Group(date_str, date_str)
                self._groups[date_str] = group
            fileinfo.group = group


class DirectoryGrouper(Grouper):

    def __init__(self) -> None:
        pass

    def __call__(self, fileinfo: 'FileInfo'):
        fileinfo.group = fileinfo.dirname()


class DurationGrouper(Grouper):

    def __init__(self) -> None:
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

    def __call__(self, fileinfo: 'FileInfo'):
        metadata = fileinfo.metadata()
        if "duration" in metadata:
            fileinfo.group = self._find_bucket(metadata["duration"] / 60000)
        else:
            fileinfo.group = None


# EOF #
