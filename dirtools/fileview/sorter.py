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


from dirtools.fileview.file_info import FileInfo


class Sorter:

    def __init__(self, controller):
        self.controller = controller
        self.directories_first = True
        self.reverse = False
        self.key_func = FileInfo.basename

    def set_directories_first(self, v):
        self.directories_first = v
        self.controller.apply_sort()
        self.controller.refresh()

    def set_sort_reversed(self, rev):
        self.reverse = rev
        self.controller.apply_sort()
        self.controller.refresh()

    def set_key_func(self, key_func):
        self.key_func = key_func
        self.controller.apply_sort()
        self.controller.refresh()

    def get_key_func(self):
        if self.directories_first:
            return lambda fileinfo: (not fileinfo.isdir(), self.key_func(fileinfo))
        else:
            return self.key_func

    def apply(self, file_collection):
        if self.key_func is None:
            file_collection.shuffle()
        else:
            file_collection.sort(self.get_key_func(), reverse=self.reverse)


# EOF #