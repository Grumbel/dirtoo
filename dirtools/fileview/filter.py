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


from fnmatch import fnmatch


class Filter:

    def __init__(self):
        self.show_hidden = False
        self.show_inaccessible = True
        self.pattern = None

    def is_filtered(self, fileinfo):
        if not self.show_hidden:
            if fileinfo.basename().startswith("."):
                return True

        if self.pattern is not None:
            if not fnmatch(fileinfo.basename(), self.pattern):
                return True

        return False

    def apply(self, file_collection):
        file_collection.filter(self.is_filtered)


# EOF #
