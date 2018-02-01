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


import random
import re
from fnmatch import fnmatch


class MatchFunc:

    def __call__(self, fileinfo, idx):
        assert False, "MatchFunc.__call__() not implemented"

    def begin(self, fileinfos):
        pass


class GlobMatchFunc(MatchFunc):

    def __init__(self, pattern):
        self.pattern = pattern

    def __call__(self, fileinfo, idx):
        return fnmatch(fileinfo.basename(), self.pattern)


class RegexMatchFunc(MatchFunc):

    def __init__(self, pattern, flags):
        self.rx = re.compile(pattern, flags)

    def __call__(self, fileinfo, idx):
        return bool(self.rx.search(fileinfo.basename()))


class TimeMatchFunc(MatchFunc):

    def __init__(self, mtime, compare):
        self.mtime = mtime
        self.compare = compare

    def __call__(self, fileinfo, idx):
        return self.compare(fileinfo.mtime(), self.mtime)


class SizeMatchFunc(MatchFunc):

    def __init__(self, size, compare):
        self.size = size
        self.compare = compare

    def __call__(self, fileinfo, idx):
        return self.compare(fileinfo.size(), self.size)


class LengthMatchFunc(MatchFunc):

    def __init__(self, length, compare):
        self.length = length
        self.compare = compare

    def __call__(self, fileinfo, idx):
        return self.compare(len(fileinfo.basename()), self.length)


class RandomMatchFunc(MatchFunc):

    def __init__(self, probability):
        self.random = random.Random(random.randrange(1024))
        self.probability = probability

    def __call__(self, fileinfo, idx):
        return (self.random.random() < self.probability)


class RandomPickMatchFunc(MatchFunc):

    def __init__(self, pick_count):
        self.pick_count = pick_count
        self.picked_indices = None

    def __call__(self, fileinfo, idx):
        return idx in self.picked_indices

    def begin(self, fileinfos):
        if self.picked_indices is not None:
            return

        if self.pick_count >= len(fileinfos):
            self.picked_indices = set(range(len(fileinfos)))
        else:
            self.picked_indices = set()
            while len(self.picked_indices) < self.pick_count:
                idx = random.randrange(len(fileinfos))
                if idx not in self.picked_indices:
                    self.picked_indices.add(idx)

        return self


class Filter:

    def __init__(self):
        self.show_hidden = False
        self.show_inaccessible = True
        self.match_func = None

    def set_regex_pattern(self, pattern, flags=0):
        self.match_func = RegexMatchFunc(pattern, flags)

    def set_pattern(self, pattern):
        self.match_func = GlobMatchFunc(pattern)

    def set_size(self, size, compare):
        self.match_func = SizeMatchFunc(size, compare)

    def set_length(self, length, compare):
        self.match_func = LengthMatchFunc(length, compare)

    def set_time(self, mtime, compare):
        self.match_func = TimeMatchFunc(mtime, compare)

    def set_random(self, probability):
        self.match_func = RandomMatchFunc(probability)

    def set_random_pick(self, count):
        self.match_func = RandomPickMatchFunc(count)

    def set_none(self):
        self.match_func = None

    def apply(self, fileinfos):
        if self.match_func is not None:
            self.match_func.begin(fileinfos)
        for idx, fileinfo in enumerate(fileinfos):
            fileinfo.is_excluded = self._is_excluded(fileinfo, idx)
            fileinfo.is_hidden = self._is_hidden(fileinfo)

    def _is_hidden(self, fileinfo):
        if not self.show_hidden:
            if fileinfo.basename().startswith("."):
                return True

        return False

    def _is_excluded(self, fileinfo, idx):
        if self.match_func is None:
            return False
        else:
            return not self.match_func(fileinfo, idx)


# EOF #
