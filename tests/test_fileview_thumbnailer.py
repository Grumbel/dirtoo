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


import unittest

from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import QApplication
from dirtools.fileview.thumbnailer import Thumbnailer


class FileViewThumbnailerTestCase(unittest.TestCase):

    def test_thumbnailer(self):
        def cb(filename, pixmap):
            print("Results:", filename, pixmap)

        try:
            app = QApplication([])
            thumbnailer = Thumbnailer()
            thumbnailer.request_thumbnail("/tmp/test.png", cb)
            QTimer.singleShot(1000, thumbnailer.close)
            QTimer.singleShot(2000, app.quit)
            app.exec()
        except Exception as err:
            print(err)

# EOF #
