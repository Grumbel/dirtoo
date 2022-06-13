# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2018 Ingo Ruhnke <grumbel@gmail.com>
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


from typing import Dict, Tuple

from pkg_resources import resource_filename

from PyQt5.QtGui import QIcon, QPixmap, QFont, QFontMetrics, QImage, QPainter

from dirtoo.fileview.image_filter import white_outline
from dirtoo.fileview.scaler import make_unscaled_rect


class SharedIcons:

    def __init__(self) -> None:
        self.folder = QIcon.fromTheme("folder")
        self.rar = QIcon.fromTheme("rar")
        self.zip = QIcon.fromTheme("zip")
        self.txt = QIcon.fromTheme("txt")
        self.image_loading = QIcon.fromTheme("image-loading")
        self.image_missing = QIcon.fromTheme("image-missing")
        self.locked = QIcon.fromTheme("locked")


class SharedPixmaps:

    def __init__(self) -> None:
        self.video = QPixmap(resource_filename("dirtoo", "fileview/icons/noun_36746_cc.png"))
        self.image = QPixmap(resource_filename("dirtoo", "fileview/icons/noun_386758_cc.png"))  # noun_757280_cc.png
        self.loading = QPixmap(resource_filename("dirtoo", "fileview/icons/noun_409399_cc.png"))
        self.error = QPixmap(resource_filename("dirtoo", "fileview/icons/noun_175057_cc.png"))
        self.locked = QPixmap(resource_filename("dirtoo", "fileview/icons/noun_236873_cc.png"))
        self.new = QPixmap(resource_filename("dirtoo", "fileview/icons/noun_258297_cc.png"))


class SharedScaleable:

    def __init__(self, width: int, height: int) -> None:
        self.width = width
        self.height = height
        self._cached_icon_pixmaps: Dict[QIcon, QPixmap] = {}

        self.folder = self.load_icon("folder", outline=True)
        self.rar = self.load_icon("rar", outline=True)
        self.zip = self.load_icon("zip", outline=True)

        self.txt = self.load_icon("txt")
        self.image_loading = self.load_icon("image-loading")
        self.image_missing = self.load_icon("image-missing")
        self.locked = self.load_icon("locked")

    def load_icon(self, name: str, outline=False) -> QPixmap:
        icon = QIcon.fromTheme(name)
        return self.load_icon_icon(icon, outline)

    def load_icon_icon(self, icon: QIcon, outline=False) -> QPixmap:
        pixmap = self._cached_icon_pixmaps.get(icon, None)
        if pixmap is not None:
            return pixmap
        else:
            pixmap = self._load_icon_icon(icon, outline)
            self._cached_icon_pixmaps[icon] = pixmap
            return pixmap

    def _load_icon_icon(self, icon: QIcon, outline=False) -> QPixmap:
        pixmap = icon.pixmap(self.width * 3 // 4, self.height * 3 // 4)

        if outline:
            icon_rect = make_unscaled_rect(self.width * 3 // 4, self.height * 3 // 4,
                                           self.width, self.height)

            img = QImage(self.width, self.height, QImage.Format_ARGB32)
            img.fill(0)

            p = QPainter(img)
            icon.paint(p, icon_rect)
            p.end()

            image = white_outline(img, sigma=6, repeat=3)

            p = QPainter(image)
            icon.paint(p, icon_rect)
            p.end()

            return QPixmap.fromImage(image)
        else:
            return pixmap


class FileViewStyle:

    def __init__(self) -> None:
        self.font = QFont("Verdana", 10)
        self.fm = QFontMetrics(self.font)
        self.shared_icons = SharedIcons()
        self.shared_pixmaps = SharedPixmaps()
        self._shared_scalable: Dict[Tuple[int, int], SharedScaleable] = {}

    def shared_scalable(self, width: int, height: int) -> QPixmap:
        key = (width, height)
        scalable = self._shared_scalable.get(key, None)
        if scalable is None:
            scalable = SharedScaleable(width, height)
            self._shared_scalable[key] = scalable
        return scalable


# EOF #
