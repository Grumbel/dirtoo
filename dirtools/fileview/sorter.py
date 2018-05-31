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


from typing import TYPE_CHECKING, Callable, Any

from dirtools.util import numeric_sort_key
from dirtools.fileview.file_info import FileInfo

if TYPE_CHECKING:
    from dirtools.fileview.file_collection import FileCollection  # noqa: F401


class Sorter:

    def __init__(self) -> None:
        self.directories_first = True
        self.reverse = False
        self.key_func: Callable[[FileInfo], Any] = lambda x: numeric_sort_key(x.basename().lower())

    def set_directories_first(self, v: bool) -> None:
        self.directories_first = v

    def set_sort_reversed(self, rev: bool) -> None:
        self.reverse = rev

    def set_key_func(self, key_func: Callable[[FileInfo], Any]) -> None:
        self.key_func = key_func

    def get_key_func(self) -> Callable[[FileInfo], Any]:
        if self.directories_first:
            return lambda fileinfo: (not fileinfo.isdir(), self.key_func(fileinfo))
        else:
            return self.key_func

    # def apply(self, file_collection: 'FileCollection') -> None:
    #     file_collection.sort(self.get_key_func(), reverse=self.reverse)


# EOF #
