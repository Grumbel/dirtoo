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


from typing import Callable

import logging
import random
import re
from fnmatch import fnmatchcase
from datetime import datetime

from dirtools.fuzzy import fuzzy

logger = logging.getLogger(__name__)


class MatchFunc:

    def __call__(self, fileinfo, idx):
        assert False, "MatchFunc.__call__() not implemented"

    def begin(self, fileinfos):
        """Called once at the start of the filtering. Can be ignored by most
        filters, but is necessary for filters than need to be aware of
        the global state (e.g. random pick filter)"""
        pass

    def cost(self):
        return 1


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


class SizeMatchFunc(MatchFunc):

    def __init__(self, size, compare):
        self.size = size
        self.compare = compare

    def __call__(self, fileinfo, idx):
        return self.compare(fileinfo.size(), self.size)


class MetadataMatchFunc(MatchFunc):

    def __init__(self, field, type, value, compare):
        self._field = field
        self._type = type
        self._value = value
        self._compare = compare

    def __call__(self, fileinfo, idx):
        metadata = fileinfo.metadata()
        if self._field in metadata:
            try:
                return self._compare(self._type(metadata[self._field]), self._value)
            except Exception as err:
                logger.exception("metadata comparism failed")
                return False
        else:
            return False

    def cost(self):
        return 50


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


class DateMatchFunc(MatchFunc):

    def __init__(self, pattern):
        self._pattern = pattern

    def __call__(self, fileinfo, idx):
        mtime = fileinfo.mtime()
        dt = datetime.fromtimestamp(mtime)
        dtstr = dt.strftime("%Y-%m-%d")
        return fnmatchcase(dtstr, self._pattern)


class TimeMatchFunc(MatchFunc):

    def __init__(self, pattern):
        self._pattern = pattern

    def __call__(self, fileinfo, idx):
        mtime = fileinfo.mtime()
        dt = datetime.fromtimestamp(mtime)
        dtstr = dt.strftime("%H:%M:%S")
        return fnmatchcase(dtstr, self._pattern)


class TimeOpMatchFunc(MatchFunc):

    def __init__(self, text: str, compare: Callable):
        self._compare = compare
        for idx, fmt in enumerate(["%H:%M:%S", "%H:%M", "%H"]):
            try:
                self._time = datetime.strptime(text, fmt).time()
                self._snip = idx
                break
            except ValueError:
                pass
        else:
            raise Exception("TimeOpMatchFunc: couldn't parse text: {}".format(text))

    def _snip_it(self, time):
        if self._snip == 0:
            return time
        elif self._snip == 1:
            return (time.hour, time.minute)
        elif self._snip == 2:
            return time.hour
        else:
            assert False

    def __call__(self, fileinfo, idx):
        mtime = fileinfo.mtime()
        dt = datetime.fromtimestamp(mtime)
        return self._compare(self._snip_it(dt.time()), self._snip_it(self._time))


class DateOpMatchFunc(MatchFunc):

    def __init__(self, text: str, compare: Callable):
        self._compare = compare
        for idx, fmt in enumerate(["%Y-%m-%d", "%Y-%m", "%Y"]):
            try:
                self._date = datetime.strptime(text, fmt).date()
                self._snip = idx
                break
            except ValueError:
                pass
        else:
            raise Exception("DateOpMatchFunc: couldn't parse text: {}".format(text))

    def _snip_it(self, date):
        if self._snip == 0:
            return date
        elif self._snip == 1:
            return (date.year, date.month)
        elif self._snip == 2:
            return date.year
        else:
            assert False

    def __call__(self, fileinfo, idx):
        mtime = fileinfo.mtime()
        dt = datetime.fromtimestamp(mtime)
        return self._compare(self._snip_it(dt.date()), self._snip_it(self._date))


class ContainsMatchFunc(MatchFunc):

    def __init__(self, text, case_sensitive):
        self._text = text
        self._case_sensitive = case_sensitive

        if not self._case_sensitive:
            self._text = self._text.lower()

    def __call__(self, fileinfo, idx):
        location = fileinfo.location()
        if not location.has_payload():
            with open(location.get_path(), "r", encoding="utf-8", errors="replace") as fin:
                for line in fin:
                    if self._case_sensitive:
                        if self._text in line:
                            return True
                    else:
                        if self._text in line.lower():
                            return True

        return False

    def cost(self):
        return 100


# EOF #
