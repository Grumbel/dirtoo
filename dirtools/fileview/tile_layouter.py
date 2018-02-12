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


import logging
from enum import Enum

from PyQt5.QtCore import QRectF

logger = logging.getLogger(__name__)


class LayoutStyle(Enum):

    ROWS = 0
    COLUMNS = 1


class TileLayouter:

    def __init__(self):
        self.padding_x = 16
        self.padding_y = 16

        self.spacing_x = 16
        self.spacing_y = 16

        self.tile_width = 128
        self.tile_height = 128 + 16

        self.viewport_width = 256
        self.viewport_height = 256

        self.col = 0
        self.row = 0

        self.columns = 1
        self.rows = 1

        self.right_x = 0
        self.bottom_y = 0

        self.layout_style = LayoutStyle.ROWS

        self.needs_relayout = True

    def set_style(self, style):
        self.layout_style = style
        self._update()

    def set_tile_size(self, tile_w, tile_h):
        self.tile_width = tile_w
        self.tile_height = tile_h
        self._update()

    def set_padding(self, x, y):
        self.padding_x = x
        self.padding_y = y
        self._update()

    def set_spacing(self, x, y):
        self.spacing_x = x
        self.spacing_y = y
        self._update()

    def _calc_num_columns(self):
        return max(1,
                   (self.viewport_width - 2 * self.padding_x + self.spacing_x) //
                   (self.tile_width + self.spacing_x))

    def _calc_num_rows(self):
        return max(1,
                   (self.viewport_height - 2 * self.padding_y + self.spacing_y) //
                   (self.tile_height + self.spacing_y))

    def _update(self):
        self.columns = self._calc_num_columns()
        self.needs_relayout = True

    def resize(self, w, h):
        self.viewport_width = w
        self.viewport_height = h

        new_columns = self._calc_num_columns()
        if self.columns != new_columns:
            self.columns = new_columns
            self.needs_relayout = True

    def get_bounding_rect(self):
        if self.layout_style == LayoutStyle.ROWS:  # center alignment
            w = self.right_x
        else:
            w = max(self.viewport_width, self.right_x)
        h = max(self.viewport_height, self.bottom_y)

        return QRectF(0, 0, w, h)

    def append_item(self, item):
        # insert new item at the current position
        x = self.col * (self.tile_width + self.spacing_x) + self.padding_x
        y = self.row * (self.tile_height + self.spacing_y) + self.padding_y

        item.set_tile_size(self.tile_width, self.tile_height)
        item.setPos(x, y)

        self.right_x = max(self.right_x, x + self.tile_width + self.padding_x)
        self.bottom_y = y + self.tile_height + self.padding_y

        # calculate next items position
        if self.layout_style == LayoutStyle.ROWS:
            self.col += 1
            if self.col == self.columns:
                self.col = 0
                self.row += 1
        else:
            self.row += 1
            if self.row == self.rows:
                self.row = 0
                self.col += 1

    def layout(self, items, force):
        if not self.needs_relayout and not force:
            logger.debug("TileLayouter.layout: skipping relayout")
            return

        if not items:
            return

        logger.debug("TileLayouter.layout: layouting")
        num_items = len(items)

        self.rows = max((num_items + self.columns - 1) // self.columns,
                        self._calc_num_rows())

        self.right_x = 0
        self.bottom_y = 0

        self.col = 0
        self.row = 0
        for item in items:
            self.append_item(item)

        self.needs_relayout = False


# EOF #
