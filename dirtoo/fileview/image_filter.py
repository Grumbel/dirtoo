#!/usr/bin/env python3

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


import numpy
import numpy.typing as npt
from scipy.ndimage.filters import gaussian_filter

from PyQt5.QtGui import QImage


def drop_shadow(image: QImage) -> QImage:
    if image.format() != QImage.Format_ARGB32:
        image = image.convertToFormat(QImage.Format_ARGB32)

    bits = image.bits()
    bits.setsize(image.byteCount())

    shape = (image.width() * image.height())
    strides = (4,)

    alpha: npt.NDArray[numpy.uint8] = numpy.ndarray(shape=shape, dtype=numpy.uint8,
                                                    buffer=bits, strides=strides, offset=3)
    color: npt.NDArray[numpy.uint8] = numpy.ndarray(shape=shape, dtype=numpy.uint8)
    color.fill(0)

    alpha = alpha.reshape((image.width(), image.height()))
    alpha = gaussian_filter(alpha, sigma=10)
    alpha = alpha.reshape(shape)

    arr = numpy.dstack((color, color, color, alpha))

    return QImage(arr, image.width(), image.height(), QImage.Format_ARGB32)


def white_outline(image: QImage, sigma: int = 6, repeat: int = 6) -> QImage:
    if image.format() != QImage.Format_ARGB32:
        image = image.convertToFormat(QImage.Format_ARGB32)

    bits = image.bits()
    bits.setsize(image.byteCount())

    shape = (image.width() * image.height())
    strides = (4,)

    alpha: npt.NDArray[numpy.uint8] = numpy.ndarray(shape=shape, dtype=numpy.uint8,
                                                    buffer=bits, strides=strides, offset=3)
    color: npt.NDArray[numpy.uint8] = numpy.ndarray(shape=shape, dtype=numpy.uint8)
    color.fill(255)

    alpha = alpha.reshape((image.width(), image.height()))

    alpha_f: npt.NDArray[numpy.single] = alpha.astype(numpy.single)
    alpha_f = gaussian_filter(alpha_f, sigma=sigma)
    alpha_f *= numpy.single(repeat)
    numpy.clip(alpha_f, 0, 255, out=alpha_f)
    alpha = alpha_f.astype(numpy.uint8)

    alpha = alpha.reshape(shape)

    arr = numpy.dstack((color, color, color, alpha))

    return QImage(arr, image.width(), image.height(), QImage.Format_ARGB32)


# EOF #
