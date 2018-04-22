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


from typing import List

import signal
import sys
import dlib
import cv2
import numpy

from PyQt5.QtCore import QRect
from PyQt5.QtGui import QImage, QPixmap, QPainter, QColor
from PyQt5.QtWidgets import QApplication, QLabel


def main(argv: List[str]) -> None:
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    image = QImage('face.jpg')
    if image.format() != QImage.Format_RGB32:
        image = image.convertToFormat(QImage.Format_RGB32)

    bits = image.bits()
    bits.setsize(image.byteCount())

    array = numpy.ndarray(shape=(image.height(), image.bytesPerLine() // 4, 4), dtype=numpy.uint8,
                          buffer=bits)
    array = array[:image.height(), :image.width(), :3]

    img = cv2.imread('face.jpg', cv2.IMREAD_COLOR)
    print(img.shape)
    print(array.shape)

    print(img)
    print()
    print(array)

    detector = dlib.get_frontal_face_detector()
    results = detector(img)
    print(results)
    results = detector(array)
    print(results)

    painter = QPainter(image)
    painter.setPen(QColor(0xffffff))
    for rect in results:
        painter.drawRect(QRect(rect.left(), rect.top(), rect.width(), rect.height()))
    painter.end()

    app = QApplication([])
    label = QLabel()
    label.setPixmap(QPixmap.fromImage(image))
    label.show()
    app.exec()


if __name__ == "__main__":
    main(sys.argv)


# EOF #
