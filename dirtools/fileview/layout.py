# dirtool.py - diff tool for directories
# Copyright (C) 2017 Ingo Ruhnke <grumbel@gmail.com>
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

from enum import Enum
from PyQt5.QtCore import QRectF

from dirtools.fileview.file_item import FileItem
from dirtools.fileview.profiler import profile


class Layout:

    def __init__(self):
        self.parent = None

        self.x = 0
        self.y = 0

        self.width = 0
        self.height = 0

    def layout(self, width):
        self.width = width

    def set_pos(self, x, y):
        self.x = x
        self.y = y

    def get_width(self):
        return self.width

    def get_height(self):
        return self.height


class VSpacer(Layout):

    def __init__(self, height):
        super().__init__()

        self.height = height


class HBoxLayout(Layout):

    def __init__(self):
        super().__init__()

        self.children: List[Layout] = []

    def add(self, child):
        self.children.append(child)
        child.parent = self

    def layout(self, width):
        super().layout(width)

        y = 0
        for child in self.children:
            child.set_pos(0, self.y + y)
            child.layout(width)
            y += child.height

        self.height = y

    def resize(self, width, height):
        self.layout(width)

    def get_bounding_rect(self):
        return QRectF(self.x, self.y, self.width, self.height)


class ItemLayout(Layout):

    def __init__(self):
        super().__init__()

        self.item = None

    def set_item(self, item):
        self.item = item

    def layout(self, width):
        if self.item is not None:
            self.height = self.item.boundingRect().height()

    def set_pos(self, x, y):
        super().set_pos(x, y)
        if self.item is not None:
            self.item.setPos(x, y)


class TileStyle:

    class Arrangement(Enum):

        ROWS = 0
        COLUMNS = 1

    def __init__(self):
        self.padding_x = 16
        self.padding_y = 16

        self.spacing_x = 16
        self.spacing_y = 16

        self.tile_width = 128
        self.tile_height = 128 + 16

        self.arrangement = TileStyle.Arrangement.ROWS

    def set_arrangement(self, arrangement):
        self.arrangement = arrangement

    def set_tile_size(self, tile_w, tile_h):
        self.tile_width = tile_w
        self.tile_height = tile_h

    def set_padding(self, x, y):
        self.padding_x = x
        self.padding_y = y

    def set_spacing(self, x, y):
        self.spacing_x = x
        self.spacing_y = y


class TileLayout(Layout):

    def __init__(self, style):
        super().__init__()

        self.style = style
        self.items: List[FileItem] = []

        self.rows = 0
        self.columns = 0

    def set_items(self, items):
        self.items = items

    def _calc_num_columns(self, viewport_width):
        return max(1,
                   (viewport_width - 2 * self.style.padding_x + self.style.spacing_x) //
                   (self.style.tile_width + self.style.spacing_x))

    def _calc_num_rows(self, viewport_height):
        return max(1,
                   (viewport_height - 2 * self.style.padding_y + self.style.spacing_y) //
                   (self.style.tile_height + self.style.spacing_y))

    def set_pos(self, x, y):
        super().set_pos(x, y)

    @profile
    def layout(self, viewport_width):
        super().layout(viewport_width)

        new_columns = self._calc_num_columns(viewport_width)
        if self.columns != new_columns:
            self.columns = new_columns
            self.needs_relayout = True

        grid_width = (self.columns * (self.style.tile_width + self.style.spacing_x)) - self.style.spacing_x + 2 * self.style.padding_x
        center_x_off = (viewport_width - grid_width) / 2

        bottom_y = 0
        right_x = 0
        col = 0
        row = 0
        for item in self.items:
            # insert new item at the current position
            x = col * (self.style.tile_width + self.style.spacing_x) + self.style.padding_x
            y = row * (self.style.tile_height + self.style.spacing_y) + self.style.padding_y

            item.set_tile_size(self.style.tile_width, self.style.tile_height)
            item.setPos(self.x + x + center_x_off,
                        self.y + y)

            right_x = max(right_x, x + self.style.tile_width + self.style.padding_x)
            bottom_y = y + self.style.tile_height + self.style.padding_y

            # calculate next items position
            if self.style.arrangement == TileStyle.Arrangement.ROWS:
                col += 1
                if col == self.columns:
                    col = 0
                    row += 1
            else:
                row += 1
                if row == self.rows:
                    row = 0
                    col += 1

        self.height = bottom_y


# EOF #
