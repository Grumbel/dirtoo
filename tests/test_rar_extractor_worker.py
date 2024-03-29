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
import signal
import tempfile
import unittest

from dirtoo.archive.extractor import ExtractorResultStatus
from dirtoo.archive.rar_extractor import RarExtractor


DATADIR = os.path.dirname(__file__)


class RarExtractorWorkerTestCase(unittest.TestCase):

    def test_rar(self) -> None:
        signal.signal(signal.SIGINT, signal.SIG_DFL)

        archive_file = os.path.join(DATADIR, "test.rar")
        outdir = tempfile.mkdtemp()

        archive_file_inc = os.path.join(DATADIR, "test_incomplete.rar")
        outdir_inc = tempfile.mkdtemp()

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

        worker = RarExtractor(archive_file, outdir)
        worker.sig_entry_extracted.connect(lambda lhs, rhs: results.append(lhs))
        result = worker.extract()
        self.assertEqual(result.status, ExtractorResultStatus.SUCCESS)
        self.assertEqual(sorted(results), sorted(expected))

        results_inc = []
        expected_inc = [
            'folder',
            '1.txt',
            '2.txt',
            '3.txt',
            '4.txt',
            'folder/1.txt',
            'folder/2.txt',
            'folder/3.txt'
        ]

        worker_inc = RarExtractor(archive_file_inc, outdir_inc)
        worker_inc.sig_entry_extracted.connect(lambda lhs, rhs: results_inc.append(lhs))
        result = worker_inc.extract()

        self.assertEqual(result.status, ExtractorResultStatus.FAILURE)
        self.assertEqual(sorted(results_inc), sorted(expected_inc))


# EOF #
