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

import io
import contextlib

from dirtoo.cmd_metadata import main


class CmdMetaDataTestCase(unittest.TestCase):

    def test_main(self):
        stdout = io.StringIO()
        stderr = io.StringIO()
        with contextlib.redirect_stdout(stdout), \
             contextlib.redirect_stderr(stderr):
            try:
                main(['dt-metadata', '--help'])
                # main(['dt-metadata', '/tmp'])
            except SystemExit as ex:
                self.assertEqual(ex.code, 0)


# EOF #
