# dirtoo - File and directory manipulation tools for Python
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


import unittest

from dirtoo.sort import numeric_sorted


class UtilTestCase(unittest.TestCase):

    def test_numeric_sorted(self) -> None:
        tests = [
            (['2', '22', '10', '1'],
             ['1', '2', '10', '22']),

            (['a9', 'a999', 'a99', 'a9999'],
             ['a9', 'a99', 'a999', 'a9999']),

            (['aaa', '999'],
             ['999', 'aaa']),

            (['a9a', 'a9z', 'a9d', 'a9m3', 'a9m5', 'a9m1'],
             ['a9a', 'a9d', 'a9m1', 'a9m3', 'a9m5', 'a9z']),
        ]

        for lhs, rhs in tests:
            self.assertEqual(numeric_sorted(lhs), rhs)


# EOF #
