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
import unittest

from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import QApplication

from dirtoo.fileview.metadata_collector import MetaDataCollector
from dirtoo.fileview.stdio_filesystem import StdioFilesystem
from dirtoo.fileview.location import Location


class MetaDataCollectorTestCase(unittest.TestCase):

    def test_collector(self):
        if not os.environ.get('DISPLAY'):
            return

        app = QApplication([])

        try:
            vfs = StdioFilesystem("/tmp")
            metadata_collector = MetaDataCollector(vfs)

            def on_metadata(filename, metadata):
                print(filename)
                print(metadata)
                print()

            metadata_collector.sig_metadata_ready.connect(on_metadata)

            metadata_collector.request_metadata(Location.from_path("dirtoo/fileview/icons/noun_409399_cc.png"))

            QTimer.singleShot(500, metadata_collector.close)
            QTimer.singleShot(1500, app.quit)

            app.exec()
        except Exception as err:
            print(err)
        finally:
            metadata_collector.close()
            vfs.close()


# EOF #
