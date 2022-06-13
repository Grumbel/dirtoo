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
import os
import time

from dirtoo.format import progressbar


class FormatTestCase(unittest.TestCase):

    def test_format(self):
        self.assertEqual(progressbar(10, 0, 10), "          ")
        self.assertEqual(progressbar(10, 10, 10), "██████████")
        self.assertEqual(progressbar(10, 5, 10), "█████     ")


# EOF #
