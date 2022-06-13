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


import os
import tempfile
import signal
import unittest
import pyparsing


from dirtoo.extractor import ExtractorResult
from dirtoo.sevenzip_extractor import SevenZipExtractor

from PyQt5.QtCore import QCoreApplication, QTimer


DATADIR = os.path.dirname(__file__)


class SevenZipExtractorWorkerTestCase(unittest.TestCase):

    def test_sevenzip(self):
        signal.signal(signal.SIGINT, signal.SIG_DFL)

        archive_file = os.path.join(DATADIR, "test.7z")
        outdir = tempfile.mkdtemp()

        worker = SevenZipExtractor(archive_file, outdir)

        results = []
        expected = [
            'folder',
            '1.txt',
            '2.txt',
            '3.txt',
            '4.txt',
            'folder/1.txt',
            'folder/2.txt',
            'folder/3.txt',
            'folder/4.txt'
        ]

        worker.sig_entry_extracted.connect(lambda lhs, rhs: results.append(lhs))
        result = worker.extract()

        self.assertEqual(result.status, ExtractorResult.SUCCESS)
        self.assertEqual(sorted(results), sorted(expected))


# EOF #
