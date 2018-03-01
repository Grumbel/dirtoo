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

from dirtools.rar_extractor_worker import RarExtractorWorker

from PyQt5.QtCore import QCoreApplication, QTimer

class RarExtractorWorkerTestCase(unittest.TestCase):

    def test_rar(self):
        signal.signal(signal.SIGINT, signal.SIG_DFL)

        app = QCoreApplication([])
        worker = RarExtractorWorker("/tmp/a.rar", "/tmp/tmp/")

        worker.sig_entry_extracted.connect(lambda lhs, rhs: print(lhs, rhs))
        worker.sig_finished.connect(app.quit)

        QTimer.singleShot(0, lambda: worker.init())
        app.exec()

        worker.close()
        del app



# EOF #
