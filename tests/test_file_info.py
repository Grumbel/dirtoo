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


import os
import unittest

from dirtoo.fileview.file_info import FileInfo
from dirtoo.fileview.lazy_file_info import LazyFileInfo


class FileInfoTestCase(unittest.TestCase):

    def test_file_info_init(self):
        fi_real = FileInfo.from_path("/tmp/")
        fi_lazy = LazyFileInfo.from_path("/tmp/")
        st = os.stat("/tmp")

        for fi in [fi_real, fi_lazy]:
            self.assertEqual(fi.abspath(), "/tmp")
            self.assertEqual(fi.basename(), "tmp")
            self.assertEqual(fi.dirname(), "/")
            self.assertEqual(fi.uid(), st.st_uid)
            self.assertEqual(fi.gid(), st.st_gid)
            self.assertEqual(fi.ext(), "")
            self.assertTrue(fi.isdir())
            self.assertFalse(fi.isfile())
            self.assertTrue(fi.have_access())
            self.assertFalse(fi.is_video())
            self.assertFalse(fi.is_image())
            self.assertFalse(fi.is_archive())


# EOF #
