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


from PyQt5.QtCore import QRect


def make_unscaled_rect(sw: int, sh: int, tw: int, th: int) -> QRect:
    return QRect(tw // 2 - sw // 2,
                 th // 2 - sh // 2,
                 sw, sh)


def make_scaled_rect(sw: int, sh: int, tw: int, th: int) -> QRect:
    tratio = tw / th
    sratio = sw / sh

    if tratio > sratio:
        w = sw * th // sh
        h = th
    else:
        w = tw
        h = sh * tw // sw

    return QRect(tw // 2 - w // 2,
                 th // 2 - h // 2,
                 w, h)


def make_cropped_rect(sw: int, sh: int, tw: int, th: int) -> QRect:
    """Calculate a srcrect that fits into the dstrect and fills it,
    cropping the src."""

    tratio = tw / th
    sratio = sw / sh

    if tratio > sratio:
        w = sw
        h = th * sw // tw

        return QRect(0,
                     0,  # top align
                     # max(0, sh // 3 - h // 2),
                     # center: sh // 2 - h // 2,
                     w, h)
    else:
        w = tw * sh // th
        h = sh

        return QRect(sw // 2 - w // 2,
                     0,
                     w, h)


# EOF #
