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


from typing import Optional

from dirtools.fileview.match_func import (
    MatchFunc,
    RegexMatchFunc,
    GlobMatchFunc,
    FuzzyMatchFunc,
    SizeMatchFunc,
    LengthMatchFunc,
    RandomMatchFunc,
    RandomPickMatchFunc,
    FolderMatchFunc,
    CharsetMatchFunc,
)


class Filter:

    def __init__(self) -> None:
        self.show_hidden = False
        self.show_inaccessible = True
        self.match_func: Optional[MatchFunc] = None

    def set_match_func(self, match_func: Optional[MatchFunc]) -> None:
        self.match_func = match_func

    def set_regex_pattern(self, pattern, flags=0) -> None:
        self.match_func = RegexMatchFunc(pattern, flags)

    def set_pattern(self, pattern, case_sensitive) -> None:
        self.match_func = GlobMatchFunc(pattern, case_sensitive=case_sensitive)

    def set_fuzzy(self, pattern) -> None:
        self.match_func = FuzzyMatchFunc(pattern)

    def set_size(self, size, compare) -> None:
        self.match_func = SizeMatchFunc(size, compare)

    def set_length(self, length, compare) -> None:
        self.match_func = LengthMatchFunc(length, compare)

    def set_random(self, probability) -> None:
        self.match_func = RandomMatchFunc(probability)

    def set_random_pick(self, count) -> None:
        self.match_func = RandomPickMatchFunc(count)

    def set_ascii(self, include) -> None:
        self.match_func = CharsetMatchFunc("ascii")

    def set_folder(self) -> None:
        self.match_func = FolderMatchFunc()

    def set_none(self) -> None:
        self.match_func = None

    def apply(self, fileinfos) -> None:
        if self.match_func is not None:
            self.match_func.begin(fileinfos)
        for idx, fileinfo in enumerate(fileinfos):
            fileinfo.is_excluded = self._is_excluded(fileinfo, idx)
            fileinfo.is_hidden = self._is_hidden(fileinfo)

    def _is_hidden(self, fileinfo) -> bool:
        if not self.show_hidden:
            if fileinfo.basename().startswith("."):
                return True

        return False

    def _is_excluded(self, fileinfo, idx: int) -> bool:
        if self.match_func is None:
            return False
        else:
            return not self.match_func(fileinfo, idx)


# EOF #
