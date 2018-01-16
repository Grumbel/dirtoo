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

import os

from PyQt5.QtCore import QRectF
from PyQt5.QtWidgets import (
    QGraphicsScene,
    QGraphicsView,
)

from dirtools.fileview.detail_file_item import DetailFileItem


class DetailView(QGraphicsView):

    def __init__(self, controller):
        super().__init__()

        self.controller = controller
        self.timespace = False
        self.file_items: List[DetailFileItem] = []

        self.scene = QGraphicsScene()
        self.setScene(self.scene)

    def set_fileinfos(self, fileinfos):
        self.scene.clear()
        self.file_items = [DetailFileItem(fileinfo, self.controller)
                           for fileinfo in fileinfos]
        for item in self.file_items:
            self.scene.addItem(item)
        self.layout_items()

    def layout_items(self):
        last_mtime = None

        if self.timespace:  # FIXME: force time sorting the ugly way
            self.file_items = sorted(self.file_items,
                                     key=lambda item: item.fileinfo.stat().st_mtime)
        y = 0
        for item in self.file_items:
            if self.timespace and last_mtime is not None:
                # FIXME: add line of varing color to signal distance
                diff = item.fileinfo.stat().st_mtime - last_mtime
                y += min(100, diff / 1000)

            item.setPos(0, y)
            y += 20
            last_mtime = item.fileinfo.stat().st_mtime

        # Calculate a bounding rect that places the items in the top/left corner
        bounding_rect = QRectF(0,
                               0,
                               max(self.viewport().size().width(),
                                   self.scene.itemsBoundingRect().width()),
                               max(self.viewport().size().height(),
                                   self.scene.itemsBoundingRect().height()))
        self.setSceneRect(bounding_rect)

    def resizeEvent(self, ev):
        super().resizeEvent(ev)
        self.layout_items()

    def toggle_timegaps(self):
        self.timespace = not self.timespace
        self.layout_items()

    # def mousePressEvent(self, ev):
    #     print("View:click")
    #     super().mousePressEvent(ev)

    def show_abspath(self):
        for item in self.file_items:
            item.show_abspath()

    def show_basename(self):
        for item in self.file_items:
            item.show_basename()

    def on_file_click(self, filename, *args):
        print("FileClick:", filename, args)

    def set_filename(self, filename):
        if filename:
            self.controller.show_current_filename(os.path.abspath(filename))
        else:
            self.controller.show_current_filename("")

# EOF #
