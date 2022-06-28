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

# from PyQt5.QtWidgets import QApplication, QPushButton
# from PyQt5.QtGui import QIcon
# from dirtoo.fileview.thumbnailer import Thumbnailer


class FileViewThumbnailerTestCase(unittest.TestCase):

    def test_thumbnailer(self) -> None:
        pass
        # try:
        #     app = QApplication([])
        #     thumbnailer = Thumbnailer()

        #     button = QPushButton()
        #     button.resize(256, 256)
        #     button.show()

        #     def quit():
        #         thumbnailer.close()
        #         app.quit()
        #     button.clicked.connect(quit)

        #     def cb(filename, flavor, pixmap):
        #         icon = QIcon(pixmap)
        #         button.setIcon(icon)
        #         button.setIconSize(pixmap.rect().size())

        #         button.repaint()

        #     thumbnailer.request_thumbnail("/tmp/test2.png", "large", cb)

        #     app.exec()

        # except Exception as err:
        #     print(err)

# EOF #
