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


import os


def expand_file(f, recursive):
    if os.path.isdir(f):
        if recursive:
            lst = [expand_file(os.path.join(f, x), recursive) for x in os.listdir(f)]
            return [item for sublist in lst for item in sublist]
        else:
            return [os.path.join(f, x) for x in os.listdir(f)]
    else:
        return [f]


def expand_directories(files, recursive):
    results = []
    for f in files:
        results += expand_file(f, recursive)
    return results


# EOF #

