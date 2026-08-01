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


from PyQt5.QtGui import QImage

from dirtools.fileview.image_filter import drop_shadow, white_outline


def main():
    image = QImage("/usr/share/icons/gnome/256x256/places/folder.png")

    output = drop_shadow(image)
    output = white_outline(image)

    outfile = "/tmp/out.png"
    print("writing output to {}".format(outfile))
    output.save(outfile)


if __name__ == "__main__":
    main()


# EOF #
