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


from typing import Callable

import logging

from PyQt5.QtCore import QObject, QSize, QRect, pyqtSignal
from PyQt5.QtGui import QBrush, QIcon, QColor, QPixmap, QImage, QPainter

from dirtools.fileview.file_collection import FileCollection
from dirtools.fileview.worker_thread import WorkerThread, Worker

if False:
    from dirtools.fileview.application import FileViewApplication  # noqa: F401
    from dirtools.fileview.location import Location  # noqa: F401

logger = logging.getLogger(__name__)


ThumbnailCallback = Callable


class DirectoryThumbnailerWorker(Worker):

    sig_thumbnail_ready = pyqtSignal(object, str, object, int, str)

    # Location, Callback
    sig_thumbnail_requested = pyqtSignal(object, object)

    def __init__(self, app: 'FileViewApplication') -> None:
        super().__init__()

        self._app = app
        self._queue = []

        self._stream = None
        self._file_collection = None

        self._thumbnails = []
        self._fileinfo_idx = 0

        self.sig_thumbnail_ready.connect(self._on_thumbnail_ready)
        self.sig_thumbnail_requested.connect(self._on_thumbnail_requested)

    def close(self) -> None:
        if self._stream is not None:
            self._stream.close()

    def _on_thumbnail_requested(self, location: 'Location', callback: ThumbnailCallback):
        print("_on_thumbnail_requested")
        logger.debug("DirectoryThumbnailer.request_thumbnail: %s", location)
        self._queue.append((location, callback))
        if self._stream is None:
            self._start_thumbnail_build(location, callback)

    def _start_thumbnail_build(self, location: 'Location', callback: ThumbnailCallback):
        assert self._stream is None
        assert self._file_collection is None

        self._stream = self._app.vfs.opendir(location)

        self._file_collection = FileCollection()

        if hasattr(self._stream, 'sig_file_added'):
            self._stream.sig_file_added.connect(self._file_collection.add_fileinfo)
        print("oeu")
        self._stream.sig_file_added.connect(lambda *args: print(*args))

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

        print("stream start: ", self._stream)
        self._stream.start()

    def _on_finished(self):
        print("_on_finished")

    def _on_scandir_finished(self, fileinfos):
        print("_on_scandir_finished:")
        self._file_collection.set_fileinfos(fileinfos)
        self._request_thumbnails()

    def _on_directory_watcher_message(self, message):
        print("ERROR:", message)

    def _request_thumbnails(self) -> None:
        print("_request_thumbnails")
        print(self._fileinfo_idx, len(self._file_collection))
        for fi in self._file_collection:
            print(fi)

        if self._fileinfo_idx < len(self._file_collection):
            print("requesting...")
            fi = self._file_collection[self._fileinfo_idx]
            self._app.thumbnailer.request_thumbnail(fi.location(), "normal", False,
                                                    lambda *args: self.sig_thumbnail_ready.emit(*args))
            self._fileinfo_idx += 1
            return True
        else:
            print("FAILURE")
            return False

    def _on_thumbnail_ready(self, location: 'Location', flavor: str, image: QImage,
                            error_code: int, message: str):
        print("_on_thumbnail_ready")
        if image is not None:
            self._thumbnails.append(image)

        if len(self._thumbnails) >= 9 or not self._request_thumbnails():
            self._build_directory_thumbnail()
        else:
            self._request_thumbnails()

    def _build_directory_thumbnail(self):
        print("_build_directory_thumbnail: ", len(self._thumbnails))
        output = QImage(QSize(256, 256), QImage.Format_RGB32)
        painter = QPainter(output)

        spec = [(0, 0, 85, 85), (85, 0, 86, 86), (171, 0, 85, 85),
                (0, 85, 86, 86), (85, 85, 86, 86), (171, 85, 86, 86),
                (0, 171, 85, 85), (85, 171, 86, 86), (171, 171, 85, 85)]

        if len(self._thumbnails) == 9:
            for idx, thumbnail in enumerate(self._thumbnails):
                dstrect = QRect(*spec[idx])
                srcrect = QRect(0, 0, thumbnail.width(), thumbnail.height())
                painter.drawImage(dstrect, thumbnail, srcrect)

        painter.end()

        output.setText("Thumb::URI", "foo")
        output.setText("Thumb::MTime", "foo")
        output.setText("Thumb::Size", "foo")
        output.setText("Thumb::Mimetype", "foo")
        output.setText("Thumb::Image::Width", "foo")
        output.setText("Thumb::Image::Height", "foo")

        output.save("/tmp/out.png")
        print("DONE")
        self._close = True


class DirectoryThumbnailer(WorkerThread):

    def __init__(self, app: 'FileViewApplication') -> None:
        worker = DirectoryThumbnailerWorker(app)
        super().__init__(worker)

    def request_thumbnail(self, location: 'Location', callback: ThumbnailCallback):
        print("request_thumbnail", location, callback)
        self._worker.sig_thumbnail_requested.emit(location, callback)


# EOF #
