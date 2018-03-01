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


import signal
import unittest
import pyparsing

from dirtools.sevenzip_extractor_worker import SevenZipExtractorWorker

from PyQt5.QtCore import QCoreApplication

class SevenZipExtractorWorkerTestCase(unittest.TestCase):

    def test_sevenzip(self):
        return
        signal.signal(signal.SIGINT, signal.SIG_DFL)
        app = QCoreApplication([])
        worker = SevenZipExtractorWorker("/tmp/a.zip", "/tmp/tmp/")
        worker.sig_entry_extracted.connect(lambda lhs, rhs: print(lhs, rhs))
        worker.sig_finished.connect(lambda: (print("DONE"), app.quit()))
        worker.init()
        print("MAINLOOP")
        app.exec()
        print("CLOSE")
        worker.close()
        del app



# EOF #
