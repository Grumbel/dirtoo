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


from datetime import datetime


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
