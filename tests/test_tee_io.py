# dirtoo - File and directory manipulation tools for Python
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


import unittest

from io import StringIO, BytesIO
from dirtoo.tee_io import TeeIO


lorem_ipsum = (
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do\n"
    "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad\n"
    "minim veniam, quis nostrud exercitation ullamco laboris nisi ut\n"
    "aliquip ex ea commodo consequat. Duis aute irure dolor in "
    "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
    "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
    "culpa qui officia deserunt mollit anim id est laborum."
)


class TeeIOTestCase(unittest.TestCase):

    def test_text_tee_io(self):
        input_io = StringIO(lorem_ipsum)
        output_io = StringIO()

        with TeeIO(input_io, output_io) as tee_io:
            while True:
                buf = tee_io.read(4)
                if not buf:
                    break

            self.assertEqual(lorem_ipsum, output_io.getvalue())

    def test_bytes_tee_io(self):
        input_io = BytesIO(lorem_ipsum.encode())
        output_io = BytesIO()

        with TeeIO(input_io, output_io) as tee_io:
            while True:
                buf = tee_io.read(4)
                if not buf:
                    break

            self.assertEqual(lorem_ipsum.encode(), output_io.getvalue())


# EOF #
