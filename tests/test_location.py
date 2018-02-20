# dirtool.py - diff tool for directories
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

from dirtools.fileview.location import Location


class LocationTestCase(unittest.TestCase):

    def test_location(self):
        ok_texts = ["file:///home/juser/test.rar//rar:file_inside.rar",
                    "file:///home/juser/test.rar",
                    "file:///home/juser/test.rar//rar",
                    "file:///tmp/",
                    "file:///home/juser/test.rar//rar:file_inside.rar//rar:file.txt",
                    "file:///test.rar//rar:one//rar:two//rar:three"]

        fail_texts = ["/home/juser/test.rar",
                      "file://test.rar",
                      "file:///test.rar//rar:oeu//"
                      "file:///test.rar///rar:foo"]

        for text in ok_texts:
            location = Location.from_url(text)
            self.assertEqual(location.as_url(), text)

        for text in fail_texts:
            with self.assertRaises(Exception) as context:
                Location.from_string(text)


# EOF #
