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


import logging

from PyQt5.QtGui import QBrush, QIcon, QColor, QPixmap, QImage

from dirtools.fileview.file_collection import FileCollection

logger = logging.getLogger(__name__)


class DirectoryThumbnailerWorker:

    pass


class DirectoryThumbnailer:

    def __init__(self, app) -> None:
        self._app = app
        self._stream = None
        self._file_collection = None
        self._thumbnails = []
        self._fileinfo_idx = 0

    def close(self) -> None:
        self._stream.close()

    def request_thumbnail(self, location: Location, flavor: str, force: bool,
                          callback: ThumbnailCallback):
        logger.debug("DirectoryThumbnailer.request_thumbnail: %s", location)

        self._file_collection = FileCollection()
        self._stream = self._app.vfs.opendir(location)

        if hasattr(self._stream, 'sig_file_added'):
            self._stream.sig_file_added.connect(self._file_collection.add_fileinfo)

        if hasattr(self._stream, 'sig_file_removed'):
            self._stream.sig_file_removed.connect(self._file_collection.remove_file)

        if hasattr(self._stream, 'sig_file_modified'):
            self._stream.sig_file_modified.connect(self._file_collection.modify_file)

        if hasattr(self._stream, 'sig_file_closed'):
            self._stream.sig_file_closed.connect(self._file_collection.close_file)

        if hasattr(self._stream, 'sig_finished'):
            self._stream.sig_finished.connect(self._on_finished)

        if hasattr(self._stream, 'sig_scandir_finished'):
            self._stream.sig_scandir_finished.connect(self._on_scandir_finished)

        if hasattr(self._stream, 'sig_message'):
            self._stream.sig_message.connect(self._on_directory_watcher_message)

        self._stream.start()

        self.sig_thumbnail_requested.emit(location)

    def _on_finished(self):
        pass

    def _on_scandir_finished(self):
        self._request_thumbnail()

    def _request_thumbnail(self):
        if self._fileinfo_idx < len(self._file_collection):
            fi = self._file_collection[self._fileinfo_idx]
            self._app.thumbnailer.request_thumbnail(fi.location(), normal, False, self._on_thumbnail_ready)
            self._fileinfo_idx += 1
            return True
        else:
            return False

    def _on_thumbnail_ready(location: Location, flavor: str, image: QImage, error_code: int, message: str):
        if image is not None:
            self._thumbnails.append(image)

        if not self._request_thumbnail() or len(self._thumbnail_requests) == 9:
            self._build_directory_thumbnail()
        else:
            self._request_thumbnail()

    def _build_directory_thumbnail(self):
        output = QImage(QSize(256, 256), QImage.Format_RGB32)
        painter = QPainter(output)

        spec = [(0, 0, 85, 85), (85, 0, 86, 86), (171, 0, 85, 85),
                (0, 85, 86, 86), (85, 85, 86, 86), (171, 85, 86, 86),
                (0, 171, 85, 85), (85, 171, 86, 86), (171, 171, 85, 85)]

        if len(self._thumbnails) == 9:
            for idx, thumbnail in enumerate(self._thumbnails):
                painter.drawPixmap(QRect(*spec[idx]), self._thumbnails[0])

        painter.end()


# EOF #
