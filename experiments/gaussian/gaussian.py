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


import numpy
from scipy.ndimage.filters import gaussian_filter

from PyQt5.QtGui import QImage


def drop_shadow(image: QImage):
    bits = image.bits()
    bits.setsize(image.byteCount())

    shape = (image.width() * image.height())
    strides = (4,)

    alpha = numpy.ndarray(shape=shape, dtype=numpy.uint8,
                          buffer=bits, strides=strides, offset=3)
    color = numpy.ndarray(shape=shape, dtype=numpy.uint8)
    color.fill(0)

    alpha = alpha.reshape((image.width(), image.height()))
    alpha = gaussian_filter(alpha, sigma=10)
    alpha = alpha.reshape(shape)

    arr = numpy.dstack((color, color, color, alpha))

    return QImage(bytes(arr), image.width(), image.height(), QImage.Format_ARGB32)


def main():
    image = QImage("/usr/share/icons/gnome/256x256/places/folder.png")

    output = drop_shadow(image)

    outfile = "/tmp/out.png"
    print("writing output to {}".format(outfile))
    output.save(outfile)


if __name__ == "__main__":
    main()


# EOF #
