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


import re
from fnmatch import fnmatch


class GlobMatchFunc:

    def __init__(self, pattern):
        self.pattern = pattern

    def __call__(self, fileinfo):
        return fnmatch(fileinfo.basename(), self.pattern)


class RegexMatchFunc:

    def __init__(self, pattern, flags):
        self.rx = re.compile(pattern, flags)

    def __call__(self, fileinfo):
        print(fileinfo.basename(), bool(self.rx.search(fileinfo.basename())))
        return bool(self.rx.search(fileinfo.basename()))


class TimeMatchFunc:

    def __init__(self, mtime, compare):
        self.mtime = mtime
        self.compare = compare

    def __call__(self, fileinfo):
        return self.compare(fileinfo.mtime(), self.mtime)


class SizeMatchFunc:

    def __init__(self, size, compare):
        self.size = size
        self.compare = compare

    def __call__(self, fileinfo):
        return self.compare(fileinfo.size(), self.size)


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

    def set_time(self, mtime, compare):
        self.match_func = TimeMatchFunc(mtime, compare)

    def set_none(self):
        self.match_func = None

    def is_hidden(self, fileinfo):
        if not self.show_hidden:
            if fileinfo.basename().startswith("."):
                return True

        return False

    def is_filtered(self, fileinfo):
        if self.match_func is None:
            return False
        else:
            return not self.match_func(fileinfo)


# EOF #
