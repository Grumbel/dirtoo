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
from fnmatch import fnmatchcase


from dirtools.fuzzy import fuzzy


class MatchFunc:

    def __call__(self, fileinfo, idx):
        assert False, "MatchFunc.__call__() not implemented"

    def begin(self, fileinfos):
        """Called once at the start of the filtering. Can be ignored by most
        filters, but is necessary for filters than need to be aware of
        the global state (e.g. random pick filter)"""
        pass


class FalseMatchFunc(MatchFunc):

    def __call__(self, fileinfo, idx):
        return False


class OrMatchFunc(MatchFunc):

    def __init__(self, funcs):
        self._funcs = funcs

    def begin(self, fileinfos):
        for func in self._funcs:
            func.begin(fileinfos)

    def __call__(self, fileinfo, idx):
        for func in self._funcs:
            if func(fileinfo, idx):
                return True
        return False


class AndMatchFunc(MatchFunc):

    def __init__(self, funcs):
        self._funcs = funcs

    def begin(self, fileinfos):
        for func in self._funcs:
            func.begin(fileinfos)

    def __call__(self, fileinfo, idx):
        for func in self._funcs:
            if not func(fileinfo, idx):
                return False
        return True


class ExcludeMatchFunc(MatchFunc):

    def __init__(self, func):
        self._func = func

    def begin(self, fileinfos):
        self._func.begin(fileinfos)

    def __call__(self, fileinfo, idx):
        return not self._func(fileinfo, idx)


class FolderMatchFunc(MatchFunc):

    def __init__(self):
        pass

    def __call__(self, fileinfo, idx):
        return fileinfo.isdir()


class GlobMatchFunc(MatchFunc):

    def __init__(self, pattern, case_sensitive=False):
        self.case_sensitive = case_sensitive
        if self.case_sensitive:
            self.pattern = pattern
        else:
            self.pattern = pattern.lower()

    def __call__(self, fileinfo, idx):
        if self.case_sensitive:
            return fnmatchcase(fileinfo.basename(), self.pattern)
        else:
            filename = fileinfo.basename().lower()
            return fnmatchcase(filename, self.pattern)


class RegexMatchFunc(MatchFunc):

    def __init__(self, pattern, flags):
        self.rx = re.compile(pattern, flags)

    def __call__(self, fileinfo, idx):
        return bool(self.rx.search(fileinfo.basename()))


class FuzzyMatchFunc(MatchFunc):

    def __init__(self, needle, n=3, threshold=0.5):
        self.needle = needle
        self.n = n
        self.threshold = threshold

    def __call__(self, fileinfo, idx):
        result = fuzzy(self.needle, fileinfo.basename(), self.n)
        return result > self.threshold


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


class AsciiMatchFunc(MatchFunc):

    def __init__(self, include=True):
        self.rx = re.compile(r'^[0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'
                             r'!"#$%&\'()*+,-./:;<=>?@\[\\\]^_`{|}~ \t\n\r\x0b\x0c]*$')
        self.include = include

    def __call__(self, fileinfo, idx):
        if self.include:
            return self.rx.match(fileinfo.basename())
        else:
            return not self.rx.match(fileinfo.basename())

# EOF #
