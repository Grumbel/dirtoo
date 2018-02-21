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


class Bookmarks:

    def __init__(self, filename: str) -> None:
        self.config_filename = filename

    def get_entries(self):
        try:
            with open(self.config_filename, "r") as fin:
                content = fin.read().splitlines()
                return sorted(content)
        except FileNotFoundError:
            return []

    def append(self, filename: str):
        entries = self.get_entries()
        if filename in entries:
            return
        else:
            with open(self.config_filename, "a") as fout:
                print(filename, file=fout)

    def remove(self, filename: str):
        entries = self.get_entries()
        with open(self.config_filename, "w") as fout:
            for entry in entries:
                if entry != filename:
                    print(entry, file=fout)


# EOF #
