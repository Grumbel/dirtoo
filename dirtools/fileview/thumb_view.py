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


from PyQt5.QtGui import QPixmap
from PyQt5.QtWidgets import (
    QGraphicsView,
    QGraphicsScene,
    QGraphicsPixmapItem,
)

from dirtools.thumbnail import make_thumbnail_filename


class ThumbView(QGraphicsView):

    def __init__(self):
        super().__init__()
        self.setAcceptDrops(True)

        self.scene = QGraphicsScene()
        self.setScene(self.scene)

    def dragMoveEvent(self, e):
        # the default implementation will check if any item in the
        # scene accept a drop event, we don't want that, so we
        # override the function to do nothing
        pass

    def dragEnterEvent(self, e):
        print(e.mimeData().formats())
        if e.mimeData().hasFormat("text/uri-list"):
            e.accept()
        else:
            e.ignore()

    def dropEvent(self, e):
        print("drag leve")
        print(e.mimeData().urls())
        # [PyQt5.QtCore.QUrl('file:///home/ingo/projects/dirtool/trunk/setup.py')]

    def add_files(self, files):
        self.files = files
        self.thumbnails = []

        for filename in self.files:
            thumbnail_filename = make_thumbnail_filename(filename)
            if thumbnail_filename:
                thumb = QGraphicsPixmapItem(QPixmap(thumbnail_filename))
                self.scene.addItem(thumb)
                self.thumbnails.append(thumb)
                thumb.setToolTip(filename)
                thumb.show()

        self.layout_thumbnails()

    def layout_thumbnails(self):
        x = 0
        y = 0
        max_height = 0
        for thumb in self.thumbnails:
            thumb.setPos(x, y)
            x += thumb.boundingRect().width()
            max_height = max(max_height, thumb.boundingRect().height())
            if x > 1024:
                y += max_height
                x = 0
                max_height = 0


# EOF #
