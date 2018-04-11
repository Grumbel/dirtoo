# dirtool.py - diff tool for directories
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


from typing import Optional

import logging
from enum import Enum

from PyQt5.QtGui import QPixmap, QImage

logger = logging.getLogger(__name__)


class ThumbnailStatus(Enum):

    INITIAL = 0
    LOADING = 1
    THUMBNAIL_ERROR = 2
    THUMBNAIL_UNAVAILABLE = 3
    THUMBNAIL_READY = 4


class Thumbnail:

    def __init__(self, flavor: str, parent_item) -> None:
        self.parent_item = parent_item
        self.pixmap: Optional[QPixmap] = None
        self.status: ThumbnailStatus = ThumbnailStatus.INITIAL
        self.flavor: str = flavor
        self.mtime: float = 0

    def get_pixmap(self) -> QPixmap:
        if self.status == ThumbnailStatus.THUMBNAIL_READY:
            assert self.pixmap is not None
            return self.pixmap
        else:
            return None

    def set_thumbnail_image(self, image: QImage) -> None:
        if image is None:
            self.status = ThumbnailStatus.THUMBNAIL_UNAVAILABLE
            self.pixmap = None
        else:
            assert not image.isNull()
            self.status = ThumbnailStatus.THUMBNAIL_READY
            self.pixmap = QPixmap(image)
            try:
                mtime_txt = image.text("Thumb::MTime")
                self.mtime = int(mtime_txt)

                # Thumb::MTime only has 1 second resolution, thus it
                # is quite possible for an thumbnail to be out of date
                # when the file was modified while the thumbnail was
                # extracting (e.g. looking at an extracting archive).
                if int(self.parent_item.fileinfo.mtime()) != self.mtime:
                    self.reset()
            except ValueError as err:
                logger.error("%s: couldn't read Thumb::MTime tag on thumbnail: %s",
                             self.parent_item.fileinfo.location(), err)

    def reset(self) -> None:
        self.pixmap = None
        self.status = ThumbnailStatus.INITIAL
        self.mtime = 0

        self.request(force=True)

    def request(self, force=False) -> None:
        assert self.status != ThumbnailStatus.LOADING

        if not self.parent_item.fileinfo.is_thumbnailable():
            self.status = ThumbnailStatus.THUMBNAIL_UNAVAILABLE
        else:
            self.status = ThumbnailStatus.LOADING
            self.parent_item.file_view.request_thumbnail(
                self.parent_item, self.parent_item.fileinfo, self.flavor, force=force)


# EOF #
