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


import os
import datetime

from PyQt5.QtWidgets import (
    QGraphicsScene,
    QGraphicsView,
)

from dirtools.fileview.file_item import DetailFileItem


class DetailView(QGraphicsView):

    def __init__(self, controller):
        super().__init__()

        self.controller = controller
        self.timespace = False
        self.file_items = []

        # self.setTransformationAnchor(QGraphicsView.AnchorViewCenter)

        self.scene = QGraphicsScene()
        self.setScene(self.scene)

    def add_files(self, files):
        self.files = files
        self.layout_files()

    def layout_files(self):
        self.scene.clear()
        self.file_items = []

        last_mtime = None

        files = [(os.path.getmtime(f), f) for f in self.files]
        files = sorted(files)

        y = 0
        for idx, (mtime, filename) in enumerate(files):

            if self.timespace:
                if last_mtime is not None:
                    diff = mtime - last_mtime
                    # print(diff)
                    y += min(100, diff / 1000)
                    # FIXME: add line of varing color to signal distance

            last_mtime = mtime

            text = self.scene.addText(datetime.datetime.fromtimestamp(mtime).strftime("%F %T"))
            text.setPos(20, y)

            text = DetailFileItem(filename, self)
            text.filename = filename
            self.scene.addItem(text)
            text.setPos(200, y)

            self.file_items.append(text)
            # print(dir(text))
            # text.mousePressEvent.connect(lambda *args, filename=filename: self.on_file_click(filename, *args))

            y += 20

        self.setSceneRect(self.scene.itemsBoundingRect())
        # print(self.scene.itemsBoundingRect())

    def toggle_timegaps(self):
        self.timespace = not self.timespace
        self.layout_files()

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
            self.controller.set_filename(os.path.abspath(filename))
        else:
            self.controller.set_filename("")

# EOF #
