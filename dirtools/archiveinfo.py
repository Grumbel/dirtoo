# dirtool.py - diff tool for directories
# Copyright (C) 2017,2018 Ingo Ruhnke <grumbel@gmail.com>
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
import libarchive
import shlex
from collections import defaultdict


class ArchiveInfo:

    @staticmethod
    def from_file(filename: str) -> 'ArchiveInfo':
        with libarchive.file_reader(filename) as entries:
            archiveinfo = ArchiveInfo(entries)
            return archiveinfo

    def __init__(self, entries: libarchive.read.ArchiveRead) -> None:
        self.file_count = 0
        self.file_types: defaultdict = defaultdict(int)
        self.directories: set = set()
        self.total_size = 0

        self.process_entries(entries)

    def process_entries(self, entries):
        for entry in entries:
            if entry.isdir:
                self.process_dir(entry)
            elif entry.isfile:
                self.process_file(entry)
            else:
                pass  # ignore other file types for now

    def process_file(self, entry):
        path = str(entry)
        dirname = os.path.dirname(path)
        basename = os.path.basename(path)
        filename, ext = os.path.splitext(basename)

        self.process_dirname(dirname)

        self.file_types[ext.lower()] += 1
        self.file_count += 1
        self.total_size += entry.size

    def process_dir(self, entry):
        self.process_dirname(str(entry))

    def process_dirname(self, path):
        if path != "":
            path = os.path.normpath(path)
            while path != "":
                self.directories.add(path)
                path = os.path.dirname(path)

    def print_summary(self):
        print("  Directories ({}):".format(len(self.directories)))
        if len(self.directories) == 0:
            print("    .")
        else:
            for d in sorted(self.directories):
                print("    {}".format(shlex.quote(d)))
        print("  Files ({}):".format(self.file_count))
        for ext, count in sorted(self.file_types.items()):
            print("  {:>5d}  {}".format(count, ext))
        print("  Size:")
        print("    Total:   {:>12} Bytes".format(self.total_size))
        print("    Average: {:>12} Bytes".format(int(self.total_size / self.file_count)))


# EOF #
