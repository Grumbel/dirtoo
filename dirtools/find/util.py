# dirtool.py - diff tool for directories
# Copyright (C) 2015-2017 Ingo Ruhnke <grumbel@gmail.com>
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


import os
import fnmatch


def size_in_bytes(filename):
    return os.lstat(filename).st_size


def name_match(filename, glob):
    return fnmatch.fnmatch(filename, glob)


def replace_item(lst, needle, replacements):
    result = []
    for i in lst:
        if i == needle:
            result += replacements
        else:
            result.append(i)
    return result


def find_files(directory, recursive, filter_op, action, topdown, maxdepth):
    assert topdown is False, "not implemented"
    assert maxdepth is None, "not implemented"

    for root, dirs, files in os.walk(directory):
        for f in files:
            if filter_op.match_file(root, f):
                action.file(root, f)

        if not recursive:
            del dirs[:]


# EOF #
