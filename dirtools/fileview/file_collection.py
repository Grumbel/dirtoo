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


from PyQt5.QtCore import QObject, pyqtSignal


class FileCollection(QObject):

    sig_file_added = pyqtSignal(str)
    sig_file_removed = pyqtSignal(str)

    def __init__(self):
        self.files = []

    def add_file(self, filename):
        self.files.append(filename)
        self.sig_file_added.emit(filename)

    def remove_file(self, filename):
        self.files.remove(filename)
        self.sig_file_removed.emit(filename)


# EOF #
