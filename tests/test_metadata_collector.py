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
from typing import Any, Dict, Optional

from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import QApplication

from dirtoo.metadata.metadata_collector import MetaDataCollector
from dirtoo.filesystem.stdio_filesystem import StdioFilesystem
from dirtoo.filesystem.location import Location


class MetaDataCollectorTestCase(unittest.TestCase):

    def test_collector(self) -> None:
        if not os.environ.get('DISPLAY'):
            return

        app = QApplication([])

        vfs: Optional[StdioFilesystem] = None
        metadata_collector: Optional[MetaDataCollector] = None
        try:
            vfs = StdioFilesystem("/tmp")
            metadata_collector = MetaDataCollector(vfs)

            def on_metadata(filename: str, metadata: Dict[str, Any]) -> None:
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
            if metadata_collector is not None:
                metadata_collector.close()
            if vfs is not None:
                vfs.close()


# EOF #
