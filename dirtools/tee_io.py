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


from io import IOBase


class TeeIO(IOBase):
    """TeeIO wraps 'input_fd' and records all data read from it to 'output_fd'"""

    def __init__(self, input_fd, output_fd):
        self._input_fd = fd
        self._output_fd = output_fd

    def close(self):
        return self._input_fd.close()

    def fileno(self):
        return self._input_fd.fileno()

    def read(self, n=-1):
        return self._input_fd.read(n)


# EOF #
