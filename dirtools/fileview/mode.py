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


from typing import Optional

import logging
from enum import Enum

from dirtools.fileview.layout import TileStyle
from dirtools.fileview.settings import settings

logger = logging.getLogger(__name__)


class FileItemStyle(Enum):

    ICON = 0
    SMALLICON = 1
    DETAIL = 2


class Mode:

    def __init__(self, parent) -> None:
        self._parent = parent

        self._item_style: Optional[FileItemStyle] = None

        self._zoom_min = 0
        self._zoom_max = 0

        self._level_of_detail_min = 0
        self._level_of_detail_max = 0

        self._tile_style = TileStyle()

    def restore(self):
        self._level_of_detail = settings.value("globals/{}_level_of_detail".format(self._item_style), 3, int)
        self._zoom_index = settings.value("globals/{}_zoom_index".format(self._item_style), 5, int)

    def update(self):
        pass

    def zoom_in(self) -> None:
        self._zoom_index += 1
        if self._zoom_index > self._zoom_max:
            self._zoom_index = self._zoom_max

        settings.set_value("globals/{}_zoom_index".format(self._item_style), self._zoom_index)

    def zoom_out(self) -> None:
        self._zoom_index -= 1
        if self._zoom_index < self._zoom_min:
            self._zoom_index = self._zoom_min

        settings.set_value("globals/{}_zoom_index".format(self._item_style), self._zoom_index)

    def less_details(self):
        self._level_of_detail -= 1
        if self._level_of_detail < self._level_of_detail_min:
            self._level_of_detail = self._level_of_detail_min

        settings.set_value("globals/{}_level_of_detail".format(self._item_style), self._level_of_detail)

    def more_details(self):
        self._level_of_detail += 1
        if self._level_of_detail > self._level_of_detail_max:
            self._level_of_detail = self._level_of_detail_max

        settings.set_value("globals/{}_level_of_detail".format(self._item_style), self._level_of_detail)


class IconMode(Mode):

    def __init__(self, parent) -> None:
        super().__init__(parent)

        self._item_style = FileItemStyle.ICON

        self._zoom_min = 0
        self._zoom_max = 11

        self._level_of_detail_min = 0
        self._level_of_detail_max = 4

        self.restore()
        self.update()

    def update(self):
        self._tile_style.set_arrangement(TileStyle.Arrangement.ROWS)
        self._tile_style.set_padding(16, 16)
        self._tile_style.set_spacing(16, 16)

        k = [0, 1, 1, 2, 3][self._level_of_detail]
        tn_width = [32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048][self._zoom_index]
        tn_height = tn_width
        self._tile_style.set_tile_size(tn_width, tn_height + 16 * k)


class ListMode(Mode):

    def __init__(self, parent) -> None:
        super().__init__(parent)

        self._item_style = FileItemStyle.SMALLICON

        self._zoom_min = 0
        self._zoom_max = 6

        self.restore()
        self.update()

    def update(self):
        self._tile_style.set_arrangement(TileStyle.Arrangement.COLUMNS)
        # self._tile_style.set_arrangement(TileStyle.Arrangement.ROWS)
        self._tile_style.set_padding(8, 8)
        self._tile_style.set_spacing(16, 8)

        # FIXME: this is a bit messy and doesn't take spacing into account properly
        column_count = (self._parent.viewport().width() // (384 + 16 + 16))
        if column_count == 0:
            column_width = self._parent.viewport().width() - 16 - 16
        else:
            column_width = (self._parent.viewport().width() / column_count) - 16 - 16

        if self._zoom_index == 0:
            self._tile_style.set_tile_size(column_width, 16)
        elif self._zoom_index in [1, 2]:
            self._tile_style.set_tile_size(column_width, 24)
        elif self._zoom_index in [3]:
            self._tile_style.set_tile_size(column_width, 32)
        elif self._zoom_index in [4]:
            self._tile_style.set_tile_size(column_width, 48)
        elif self._zoom_index in [5]:
            self._tile_style.set_tile_size(column_width, 64)
        else:
            self._tile_style.set_tile_size(column_width, 128)


class DetailMode(Mode):

    def __init__(self, parent) -> None:
        super().__init__(parent)

        self._item_style = FileItemStyle.DETAIL

        self.restore()
        self.update()

    def update(self):
        self._tile_style.set_arrangement(TileStyle.Arrangement.ROWS)
        self._tile_style.set_padding(24, 8)
        self._tile_style.set_spacing(16, 8)
        self._tile_style.set_tile_size(self._parent.viewport().width() - 48, 24)


# EOF #
