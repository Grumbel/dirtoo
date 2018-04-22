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

from PyQt5.QtCore import Qt, QRect
from PyQt5.QtGui import QImage, QPixmap, QPainter, QColor, QBrush
from PyQt5.QtWidgets import QApplication, QLabel


# Dlib's face detection can't handle small faces (< 80x80), scaling
# the image up makes it work better, but it still misses a lot at 4x.
# Doesn't seem suitable to operate directly on thumbnails.
scale_factor = 2


def run_facedetect(filename: str) -> None:
    image = QImage(filename)
    if image.format() != QImage.Format_RGB32:
        image = image.convertToFormat(QImage.Format_RGB32)

    image = image.scaled(image.width() * scale_factor,
                         image.height() * scale_factor,
                         Qt.IgnoreAspectRatio,
                         Qt.SmoothTransformation)

    bits = image.bits()
    bits.setsize(image.byteCount())

    array = numpy.ndarray(shape=(image.height(), image.bytesPerLine() // 4, 4), dtype=numpy.uint8,
                          buffer=bits)
    array = array[:image.height(), :image.width(), :3]

    img = cv2.imread(filename, cv2.IMREAD_COLOR)
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

    print("detected {} faces".format(len(results)))

    image = image.scaled(image.width() // scale_factor,
                         image.height() // scale_factor)

    return image, results

def main(argv: List[str]) -> None:
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    app = QApplication([])
    print(sys.argv[1:])
    gui = []
    for image_filename in sys.argv[1:] or ["face.jpg"]:
        image, results = run_facedetect(image_filename)

        painter = QPainter(image)
        painter.setPen(QColor(0xffffff))
        for rect in results:
            painter.fillRect(QRect(rect.left() // scale_factor,
                                   rect.top() // scale_factor,
                                   rect.width() // scale_factor,
                                   rect.height() // scale_factor),
                             QBrush(QColor(255, 0, 255)))
        painter.end()

        label = QLabel()
        label.setPixmap(QPixmap.fromImage(image))
        label.show()
        gui.append(label)

    app.exec()


if __name__ == "__main__":
    main(sys.argv)


# EOF #
