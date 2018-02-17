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



class Layout:

    def __init__(self, parent):
        self.parent = parent

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


class HBoxLayout(Layout):

    def __init__(self, parent):
        super().__init__(parent)

        self.children = []

    def add(self, child):
        self.children.append(child)

    def layout(self, width):
        super().layout(width)

        y = 0
        for child in children:
            child.set_pos(0, self.y + y)
            child.layout(width)
            y += child.height(width)

        self.height = y


class ItemLayout(Layout):

    def __init__(self, parent):
        super().__init__(parent)

        self.item = None

    def set_item(self, item):
        self.item = item

    def layout(self, width):
        if self.item is not None:
            self.height = self.item.height()

    def set_pos(self, x, y):
        super().set_pos(x, y)
        if self.item is not None:
            self.item.setPos(x, y)


class TileLayout(Layout):

    class Style(Enum):

        ROWS = 0
        COLUMNS = 1

    def __init__(self, parent):
        super().__init__(parent)

        self.padding_x = 16
        self.padding_y = 16

        self.spacing_x = 16
        self.spacing_y = 16

        self.tile_width = 128
        self.tile_height = 128 + 16

        self.layout_style = TileLayoutStyle.ROWS

        self.items = []

    def set_items(self, items):
        self.items = items

    def _update(self):
        self.columns = self._calc_num_columns()
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

    def layout(self, width):
        super().layout(width)

        new_columns = self._calc_num_columns()
        if self.columns != new_columns:
            self.columns = new_columns
            self.needs_relayout = True

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


# EOF #
