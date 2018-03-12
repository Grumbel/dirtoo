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
from PyQt5.QtWidgets import QGraphicsItem

from dirtools.fileview.layout import RootLayout, HBoxLayout, TileLayout, ItemLayout, VSpacer
from dirtools.fileview.file_info import FileInfo
from dirtools.fileview.thumb_file_item import ThumbFileItem


class LayoutBuilder:

    def __init__(self, scene, style) -> None:
        self._scene = scene
        self._style = style
        self._show_filtered = False

    def _group_items(self, items):
        groups: Dict[Hashable, List[FileInfo]] = {}
        for item in items:
            if item.fileinfo.group not in groups:
                groups[item.fileinfo.group] = []
            groups[item.fileinfo.group].append(item)
        return groups

    def _build_group_title(self, title: str) -> ItemLayout:
        text_item = self._scene.addText(title, QFont("Verdana", 12))
        group_title = ItemLayout()
        group_title.set_item(text_item)
        return group_title

    def _build_tile_grid(self, items: List[QGraphicsItem], group) -> TileLayout:
        tile_layout = TileLayout(self._style, group=group)
        tile_layout.set_items(items)
        return tile_layout

    def cleanup(self):
        # ThumbFileItem's are recycled between layouts
        for item in self._scene.items():
            if not isinstance(item, ThumbFileItem):
                self._scene.removeItem(item)

    def build_layout(self, items: List[Any]) -> RootLayout:
        self.cleanup()

        hbox = HBoxLayout()

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

        grid = None
        for idx, (group, items) in enumerate(sorted_groups):
            if self._show_filtered:
                visible_items = [item for item in items if not item.fileinfo.is_hidden]
            else:
                visible_items = [item for item in items if item.fileinfo.is_visible]

            if visible_items == []:
                continue

            if group is not None:
                title = self._build_group_title(str(group))
                hbox.add(title)

            grid = self._build_tile_grid(visible_items, len(sorted_groups) > 1)

            hbox.add(grid)

            if idx + 1 != len(sorted_groups):
                spacer = VSpacer(48)
                hbox.add(spacer)

        if len(sorted_groups) <= 1:
            if grid is None:
                append_layout = TileLayout(self._style, group=False)
                hbox.add(append_layout)
            else:
                append_layout = grid
        else:
            hbox.add(self._build_group_title("Incoming"))
            append_layout = TileLayout(self._style, group=True)
            hbox.add(append_layout)

        root = RootLayout()
        root.set_root(hbox)
        root.set_append_layout(append_layout)
        return root


# EOF #
