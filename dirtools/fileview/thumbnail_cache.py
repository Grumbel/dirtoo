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


from typing import Dict, Tuple

import os
import urllib.parse
from enum import Enum

from PyQt5.QtCore import QObject, pyqtSignal

from dirtools.thumbnailer import Thumbnailer, ThumbnailerListener


class ThumbnailStatus(Enum):

    NONE = 0
    LOADING = 1
    ERROR = 2
    OK = 3


class ThumbnailCacheListener(ThumbnailerListener):

    def __init__(self, thumbnail_cache):
        self.thumbnail_cache = thumbnail_cache

    def started(self, handle):
        pass

    def ready(self, handle, urls, flavor):
        for url in urls:
            filename = urllib.parse.unquote(urllib.parse.urlparse(url).path)
            self.thumbnail_cache.put(filename,
                                     flavor,
                                     Thumbnailer.thumbnail_from_filename(filename, flavor),
                                     ThumbnailStatus.OK)

    def error(self, handle, failed_uris, error_code, message):
        for url in failed_uris:
            filename = urllib.parse.unquote(urllib.parse.urlparse(url).path)
            self.thumbnail_cache.put(filename, "large",
                                     None,
                                     ThumbnailStatus.ERROR)
            self.thumbnail_cache.put(filename, "normal",
                                     None,
                                     ThumbnailStatus.ERROR)

    def finished(self, handle):
        pass

    def idle(self):
        pass


class ThumbnailCache(QObject):

    sig_thumbnail = pyqtSignal(str, str, str, ThumbnailStatus)

    def __init__(self, thumbnailer):
        super().__init__()
        self.cache: Dict[Tuple[str, str], Tuple[str, ThumbnailStatus]] = {}
        self.thumbnailer = thumbnailer

    def get(self, fileinfo, flavor):
        result = self.cache.get((fileinfo.abspath(), flavor), None)

        if result is not None:
            return result
        else:
            thumbnail_path = Thumbnailer.thumbnail_from_filename(fileinfo.abspath(), flavor)
            if os.path.exists(thumbnail_path):
                self.cache[(fileinfo.abspath(), flavor)] = (thumbnail_path, ThumbnailStatus.OK)
                return (thumbnail_path, ThumbnailStatus.OK)
            else:
                self.cache[(fileinfo.abspath(), flavor)] = None
                self.thumbnailer.queue([fileinfo.abspath()], flavor=flavor)
                return (thumbnail_path, ThumbnailStatus.LOADING)

    def put(self, filename, flavor, thumbnail_filename, thumbnail_status):
        self.cache[(filename, flavor)] = (thumbnail_filename, thumbnail_status)
        self.sig_thumbnail.emit(filename, flavor, thumbnail_filename, thumbnail_status)


# EOF #
