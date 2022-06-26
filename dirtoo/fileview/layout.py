# dirtoo - File and directory manipulation tools for Python
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


from typing import TYPE_CHECKING, List, Optional

import math
from enum import Enum
from PyQt5.QtCore import QRectF
from PyQt5.QtWidgets import QGraphicsItem

from dirtoo.fileview.profiler import profile

if TYPE_CHECKING:
    from dirtoo.fileview.file_item import FileItem


class Layout:

    def __init__(self) -> None:
        self.parent: Optional[Layout] = None

        self.x: int = 0
        self.y: int = 0

        self.width: int = 0
        self.height: int = 0

    def layout(self, width: int, height: int) -> None:
        self.width = width
        self.height = height

    def set_pos(self, x: int, y: int) -> None:
        self.x = x
        self.y = y

    def get_width(self) -> int:
        return self.width

    def get_height(self) -> int:
        return self.height

    def get_bounding_rect(self) -> QRectF:
        return QRectF(self.x, self.y, self.width, self.height)


class VSpacer(Layout):

    def __init__(self, height: int) -> None:
        super().__init__()
        self.height: int = height

    def layout(self, width: int, height: int) -> None:
        self.width = width


class RootLayout(Layout):
    """Root of the layout tree. Holds one layout that represents the
    actual root layout and a pointer to the layout that will take newly
    arriving items."""

    def __init__(self) -> None:
        super().__init__()
        self.root: Optional[Layout] = None
        self.append_layout: Optional[TileLayout] = None

    def set_root(self, root: Layout) -> None:
        assert self.root is None
        self.root = root
        self.root.parent = self

    def set_append_layout(self, group: 'TileLayout') -> None:
        self.append_layout = group

    def layout(self, viewport_width: int, viewport_height: int) -> None:
        assert self.root is not None
        assert self.root.layout is not None

        super().layout(viewport_width, viewport_height)

        self.root.set_pos(0, 0)
        self.root.layout(viewport_width, viewport_height)

    def get_bounding_rect(self) -> QRectF:
        assert self.root is not None
        return self.root.get_bounding_rect()

    def append_item(self, item: 'FileItem') -> None:
        assert self.root is not None
        assert self.append_layout is not None

        self.append_layout.append_item(item)
        self.root.layout(self.width, self.height)


class HBoxLayout(Layout):

    def __init__(self) -> None:
        super().__init__()

        self.children: List[Layout] = []

    def add(self, child: Layout) -> None:
        self.children.append(child)
        child.parent = self

    def layout(self, viewport_width: int, viewport_height: int) -> None:
        super().layout(viewport_width, viewport_height)

        y = 0
        for child in self.children:
            child.set_pos(0, self.y + y)
            child.layout(viewport_width, viewport_height)
            y += child.height

        self.height = y

    def resize(self, width: int, height: int) -> None:
        self.layout(width, height)


class ItemLayout(Layout):
    """Layout used to hold a QGraphicsItem, e.g. the text title of a
    group."""

    def __init__(self) -> None:
        super().__init__()

        self.item: Optional[QGraphicsItem] = None

    def set_item(self, item: QGraphicsItem) -> None:
        self.item = item

    def layout(self, viewport_width: int, viewport_height: int) -> None:
        super().layout(viewport_width, viewport_height)

        if self.item is not None:
            self.height = int(self.item.boundingRect().height())

    def set_pos(self, x: int, y: int) -> None:
        super().set_pos(x, y)
        if self.item is not None:
            self.item.setPos(x, y)


class TileStyle:

    class Arrangement(Enum):

        ROWS = 0
        COLUMNS = 1

    def __init__(self) -> None:
        self.padding_x = 16
        self.padding_y = 16

        self.spacing_x = 16
        self.spacing_y = 16

        self.tile_width = 128
        self.tile_height = 128 + 16

        self.arrangement = TileStyle.Arrangement.ROWS

    def set_arrangement(self, arrangement: 'TileStyle.Arrangement') -> None:
        self.arrangement = arrangement

    def set_tile_size(self, tile_w: int, tile_h: int) -> None:
        self.tile_width = tile_w
        self.tile_height = tile_h

    def set_padding(self, x: int, y: int) -> None:
        self.padding_x = x
        self.padding_y = y

    def set_spacing(self, x: int, y: int) -> None:
        self.spacing_x = x
        self.spacing_y = y


class TileLayout(Layout):

    def __init__(self, style: TileStyle, group: bool) -> None:
        super().__init__()

        self.style = style
        self.group = group

        self.items: List[QGraphicsItem] = []

        self.rows = 0
        self.columns = 0

        self.next_col = 0
        self.next_row = 0

        self.center_x_off = 0

    def set_items(self, items: List[QGraphicsItem]) -> None:
        self.items = items

    def append_item(self, item: QGraphicsItem) -> None:
        self.items.append(item)

        col = self.next_col
        row = self.next_row

        x = col * (self.style.tile_width + self.style.spacing_x) + self.style.padding_x
        y = row * (self.style.tile_height + self.style.spacing_y) + self.style.padding_y

        item.setPos(self.x + x + self.center_x_off,
                    self.y + y)

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

        self.next_col = col
        self.next_row = row
        self.height = bottom_y

    def _calc_num_columns(self, viewport_width: int) -> int:
        return max(1,
                   (viewport_width - 2 * self.style.padding_x + self.style.spacing_x) //
                   (self.style.tile_width + self.style.spacing_x))

    def _calc_num_rows(self, viewport_height: int) -> int:
        return max(1,
                   (viewport_height - 2 * self.style.padding_y + self.style.spacing_y) //
                   (self.style.tile_height + self.style.spacing_y))

    def _calc_grid_width(self, columns: int) -> int:
        return ((columns * (self.style.tile_width + self.style.spacing_x)) -
                self.style.spacing_x + 2 * self.style.padding_x)

    def set_pos(self, x: int, y: int) -> None:
        super().set_pos(x, y)

    @profile
    def layout(self, viewport_width: int, viewport_height: int) -> None:
        super().layout(viewport_width, viewport_height)

        new_columns = self._calc_num_columns(viewport_width)
        grid_width = self._calc_grid_width(new_columns)

        self.center_x_off = (viewport_width - grid_width) // 2
        self.columns = new_columns
        self.rows = self._calc_num_rows(viewport_height)

        if len(self.items) > (self.columns * self.rows) or self.group:
            self.rows = math.ceil(len(self.items) / self.columns)

        bottom_y = 0
        right_x = 0
        col = 0
        row = 0
        for item in self.items:
            # insert new item at the current position
            x = col * (self.style.tile_width + self.style.spacing_x) + self.style.padding_x
            y = row * (self.style.tile_height + self.style.spacing_y) + self.style.padding_y

            item.setPos(self.x + x + self.center_x_off,
                        self.y + y)

            right_x = max(right_x, x + self.style.tile_width + self.style.padding_x)
            bottom_y = max(bottom_y, y + self.style.tile_height + self.style.padding_y)

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

        self.next_row = row
        self.next_col = col
        self.height = bottom_y


# EOF #
