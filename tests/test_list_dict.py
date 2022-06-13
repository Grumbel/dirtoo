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

from dirtoo.list_dict import ListDict


class ListDictTestCase(unittest.TestCase):

    def test_listdict_iter(self):
        listdict = ListDict(lambda x: x,
                            iterable=[11, 22, 33, 44, 55, 66])
        self.assertEqual(list(listdict), [11, 22, 33, 44, 55, 66])

    def test_listdict_remove(self):
        listdict = ListDict(lambda x: x,
                            iterable=[11, 22, 33, 44, 55, 66])
        listdict.remove(33)
        self.assertEqual(list(listdict), [11, 22, 44, 55, 66])

    def test_listdict_replace(self):
        listdict = ListDict(lambda x: x,
                            iterable=[11, 22, 33, 44, 55, 66])
        listdict.replace(22, 99)
        self.assertEqual(list(listdict), [11, 99, 33, 44, 55, 66])

    def test_listdict_sort(self):
        listdict = ListDict(lambda x: x,
                            iterable=[22, 11, 77, 66, 44, 55, 33])
        listdict.sort(key=lambda x: x)
        self.assertEqual(list(listdict), [11, 22, 33, 44, 55, 66, 77])

        listdict.sort(key=lambda x: x, reverse=True)
        self.assertEqual(list(listdict), [77, 66, 55, 44, 33, 22, 11])

    def test_listdict_get_by_key(self):
        listdict = ListDict(lambda x: x,
                            iterable=[22, 11, 77, 66, 44, 55, 33])
        self.assertEqual(listdict.get(55), 55)

    def test_listdict_append(self):
        listdict = ListDict(lambda x: x,
                            iterable=[22, 11, 77, 66, 44, 55, 33])
        listdict.append(99)
        self.assertEqual(list(listdict), [22, 11, 77, 66, 44, 55, 33, 99])

    def test_listdict_clear(self):
        listdict = ListDict(lambda x: x,
                            iterable=[22, 11, 77, 66, 44, 55, 33])
        listdict.clear()
        self.assertEqual(list(listdict), [])

    def test_listdict_len(self):
        listdict = ListDict(lambda x: x,
                            iterable=[22, 11, 77, 66, 44, 55, 33])
        self.assertEqual(len(listdict), 7)



# EOF #
