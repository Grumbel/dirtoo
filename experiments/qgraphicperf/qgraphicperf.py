#!/usr/bin/env python3

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


import os
import signal
import sys
import numpy
import random

from PyQt5.QtGui import QPixmapCache,QImage, QPixmap
from PyQt5.QtWidgets import (QApplication, QGraphicsView, QGraphicsItem,
                             QGraphicsScene, QGraphicsPixmapItem)


from dirtools.dbus_thumbnailer import DBusThumbnailer


def main(argv):
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    app = QApplication([])
    scene = QGraphicsScene()
    view = QGraphicsView()

    thumbnails = []
    for filename in os.listdir(argv[1]):
        filename = os.path.join(argv[1], filename)
        print(filename)
        thumbnails.append(DBusThumbnailer.thumbnail_from_filename(filename, "large"))

    count = 0
    items = []
    for y in range(0, 100000, 150):
        for x in range(0, 2500, 150):
            scene.addRect(x, y, 128, 128)


            # image = QImage(128, 128, QImage.Format_RGB32)
            if count < len(thumbnails):
                print(thumbnails[count])
                image = QImage(thumbnails[count])
            else:
                arr = numpy.random.randint(0, 2**32, (128, 128), dtype=numpy.uint32)
                image = QImage(arr, 128, 128, 128 * 4, QImage.Format_ARGB32)
            pixmap = QPixmap.fromImage(image)

            item = QGraphicsPixmapItem(pixmap)
            scene.addItem(item)

            text = scene.addText("Test Textual: {}".format(count))
            item.setPos(x, y)
            text.setPos(x, y + 128)
            count += 1

            item.setFlags(QGraphicsItem.ItemIsSelectable)
            item.setAcceptHoverEvents(True)

            items.append([item, text])
    print(count)

    if False:
        random.shuffle(items)
        i = 0
        for y in range(0, 100000, 150):
            for x in range(0, 2500, 150):
                for item in items[i]:
                        item.setPos(x, y)
                i += 1

    view.setScene(scene)
    view.resize(800, 600)
    view.show()
    app.exec()


if __name__ == "__main__":
    main(sys.argv)


# EOF #
