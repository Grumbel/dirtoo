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


from typing import Dict, List, Any, Hashable

from PyQt5.QtGui import QFont

from dirtools.fileview.layout import HBoxLayout, TileLayout, ItemLayout, VSpacer
from dirtools.fileview.file_info import FileInfo
from dirtools.fileview.thumb_file_item import ThumbFileItem


class Layouter:

    def __init__(self, scene):
        self.scene = scene

        self.padding_x = 16
        self.padding_y = 16

        self.spacing_x = 16
        self.spacing_y = 16

        self.tile_width = 128
        self.tile_height = 128 + 16

        self.show_grouping = True
        self.show_filtered = False

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

    def _update(self):
        pass

    def _group_items(self, items):
        groups: Dict[Hashable, List[FileInfo]] = {}
        for item in items:
            if item.fileinfo.group not in groups:
                groups[item.fileinfo.group] = []
            groups[item.fileinfo.group].append(item)
        return groups

    def _build_group_title(self, title: str) -> ItemLayout:
        text_item = self.scene.addText(title, QFont("Verdana", 12))
        group_title = ItemLayout()
        group_title.set_item(text_item)
        return group_title

    def _build_tile_grid(self, items: List[Any]) -> TileLayout:
        if self.show_filtered:
            visible_items = [item for item in items if not item.fileinfo.is_hidden]
        else:
            visible_items = [item for item in items if item.fileinfo.is_visible]

        tile_layout = TileLayout()
        tile_layout.set_items(visible_items)
        return tile_layout

    def cleanup(self):
        # ThumbFileItem's are recycled between layouts
        for item in self.scene.items():
            if not isinstance(item, ThumbFileItem):
                self.scene.removeItem(item)

    def build_layout(self, items: List[Any]) -> HBoxLayout:
        self.cleanup()

        self.root = HBoxLayout()

        if self.show_grouping:
            groups = self._group_items(items)

            if None in groups:
                first_group = (None, groups[None])
                del groups[None]
            else:
                first_group = None

            sorted_groups = sorted(groups.items(),
                                   key=lambda x: x[0],
                                   reverse=True)

            if first_group is not None:
                sorted_groups = [first_group] + sorted_groups

            for idx, (group, items) in enumerate(sorted_groups):
                if group is not None:
                    title = self._build_group_title(str(group))
                    self.root.add(title)

                grid = self._build_tile_grid(items)
                self.root.add(grid)

                if idx + 1 != len(sorted_groups):
                    spacer = VSpacer(48)
                    print(repr(spacer))
                    self.root.add(spacer)
        else:
            grid = self._build_tile_grid(items)
            self.root.add(grid)

        return self.root

    def append_item(self, item):
        print("append_item not implemented")


# EOF #
