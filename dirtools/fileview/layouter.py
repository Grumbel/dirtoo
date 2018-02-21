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

    def __init__(self, scene, style):
        self.scene = scene
        self.style = style
        self.show_filtered = False

        self.append_layout = None

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
        tile_layout = TileLayout(self.style)
        tile_layout.set_items(items)
        return tile_layout

    def cleanup(self):
        # ThumbFileItem's are recycled between layouts
        for item in self.scene.items():
            if not isinstance(item, ThumbFileItem):
                self.scene.removeItem(item)

    def build_layout(self, items: List[Any]) -> HBoxLayout:
        self.cleanup()

        self.root = HBoxLayout()

        if items == []:
            return self.root

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
            if self.show_filtered:
                visible_items = [item for item in items if not item.fileinfo.is_hidden]
            else:
                visible_items = [item for item in items if item.fileinfo.is_visible]

            if visible_items == []:
                continue

            if group is not None:
                title = self._build_group_title(str(group))
                self.root.add(title)

            grid = self._build_tile_grid(visible_items)

            self.root.add(grid)

            if idx + 1 != len(sorted_groups):
                spacer = VSpacer(48)
                self.root.add(spacer)

        if len(sorted_groups) == 1:
            self.append_layout = grid
        else:
            self.root.add(self._build_group_title("Incoming"))
            self.append_layout = TileLayout(self.style)
            self.root.add(self.append_layout)

        return self.root

    def append_item(self, item):
        self.append_layout.append_item(item)

    def clear_appends(self):
        self.append_layout = TileLayout(self.style)

    def resize(self, width, height):
        if self.root is not None:
            self.root.resize(width, height)


# EOF #
