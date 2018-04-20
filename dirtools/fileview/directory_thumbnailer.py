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
import os

from PyQt5.QtCore import QObject, QSize, QRect, pyqtSignal
from PyQt5.QtGui import QBrush, QIcon, QColor, QPixmap, QImage, QPainter

from dirtools.fileview.file_collection import FileCollection
from dirtools.fileview.worker_thread import WorkerThread, Worker
from dirtools.fileview.scaler import make_cropped_rect
from dirtools.dbus_thumbnailer import DBusThumbnailer

if False:
    from dirtools.fileview.application import FileViewApplication  # noqa: F401
    from dirtools.fileview.location import Location  # noqa: F401

logger = logging.getLogger(__name__)


ThumbnailCallback = Callable


class DirectoryThumbnailerTask(QObject):

    sig_thumbnail_ready = pyqtSignal(object, str, object, int, str)
    sig_done = pyqtSignal()

    def __init__(self, app: 'FileViewApplication',
                 location: 'Location', callback: ThumbnailCallback):
        super().__init__()

        self._stream = None
        self._file_collection = None

        self._app = app
        self.sig_thumbnail_ready.connect(self._on_thumbnail_ready)

        self._location = location
        if location.has_stdio_name():
            self._fileinfo = self._app.vfs.get_fileinfo(location)
        else:
            self._fileinfo = self._app.vfs.get_fileinfo(location.origin())
        self._stream = self._app.vfs.opendir(location)

        self._thumbnails = []
        self._fileinfo_idx = 0

        self._file_collection = FileCollection()

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

        # print("stream start: ", self._stream)
        self._stream.start()

    def close(self) -> None:
        self._stream.close()

    def _on_finished(self):
        # print("_on_finished")
        pass

    def _on_scandir_finished(self, fileinfos):
        # print("_on_scandir_finished:")
        self._file_collection.set_fileinfos(fileinfos)
        if not self._request_thumbnails():
            print("directory seems empty, no thumbnails to generate (might be archive, see file_collection.add_file)")
            self.sig_done.emit()

    def _on_directory_watcher_message(self, message):
        print("ERROR:", message)

    def _request_thumbnails(self) -> None:
        # print("_request_thumbnails")
        # print(self._fileinfo_idx, len(self._file_collection))

        if self._fileinfo_idx < len(self._file_collection):
            # print("requesting...")
            fi = self._file_collection[self._fileinfo_idx]
            self._app.thumbnailer.request_thumbnail(fi.location(), "large", False,
                                                    lambda *args: self.sig_thumbnail_ready.emit(*args))
            self._fileinfo_idx += 1
            return True
        else:
            # print("FAILURE")
            return False

    def _on_thumbnail_ready(self, location: 'Location', flavor: str, image: QImage,
                            error_code: int, message: str):
        # print("_on_thumbnail_ready")
        if image is not None:
            self._thumbnails.append(image)

        if len(self._thumbnails) >= 9:
            self._build_directory_thumbnail()
        else:
            if not self._request_thumbnails():
                self._build_directory_thumbnail()

    def _build_directory_thumbnail(self):
        # print("_build_directory_thumbnail: ", len(self._thumbnails))
        output = QImage(QSize(256, 256), QImage.Format_ARGB32)
        output.fill(0)
        painter = QPainter(output)
        painter.setRenderHints(QPainter.SmoothPixmapTransform |
                               QPainter.Antialiasing)

        # FIXME: insert code to handle more image numbers
        spec = [(0, 0, 85, 85), (85, 0, 86, 86), (171, 0, 85, 85),
                (0, 85, 86, 86), (85, 85, 86, 86), (171, 85, 86, 86),
                (0, 171, 85, 85), (85, 171, 86, 86), (171, 171, 85, 85)]

        for idx, thumbnail in enumerate(self._thumbnails[:9]):
            dstrect = QRect(*spec[idx])
            srcrect = make_cropped_rect(thumbnail.width(), thumbnail.height(), spec[idx][2], spec[idx][3])
            painter.drawImage(dstrect, thumbnail, srcrect)

        painter.end()

        # FIXME: this is fishy, need something in Location to handle
        # "//archive", as the thumbnail needs to be created for
        # "foo.zip" not "foo.zip//archive"
        loc = self._location.origin()
        if loc is None:
            loc = self._location

        url = loc.as_url()

        output.setText("Thumb::URI", url)
        output.setText("Thumb::MTime", str(int(self._fileinfo.mtime())))
        output.setText("Thumb::Size", str(self._fileinfo.size()))
        output.setText("Thumb::Mimetype", "inode/directory")
        output.setText("Thumb::Image::Width", "256")
        output.setText("Thumb::Image::Height", "256")

        thumbnail_filename = DBusThumbnailer.thumbnail_from_url(url, "large")
        print("Wrote thumbnail to {}".format(thumbnail_filename))
        # if not os.path.exists(thumbnail_filename):
        output.save(thumbnail_filename)
        self.sig_done.emit()


class DirectoryThumbnailerWorker(Worker):

    # Location, Callback
    sig_thumbnail_requested = pyqtSignal(object, object)

    def __init__(self, app: 'FileViewApplication') -> None:
        super().__init__()

        self._app = app

        self._task = None
        self._queue = []

        self.sig_thumbnail_requested.connect(self._on_thumbnail_requested)

    def close(self) -> None:
        if self._task is not None:
            self._task.close()
            self._task = None

    def _on_thumbnail_requested(self, location: 'Location', callback: ThumbnailCallback):
        # print("_on_thumbnail_requested")
        logger.debug("DirectoryThumbnailer.request_thumbnail: %s", location)

        if self._task is None:
            self._start_task(location, callback)
        else:
            self._queue.append((location, callback))

    def _start_task(self, location: 'Location', callback: ThumbnailCallback):
        assert self._task == None
        self._task = DirectoryThumbnailerTask(self._app, location, callback)
        self._task.sig_done.connect(self._on_task_done)

    def _on_task_done(self):
        print("DONE")

        self._task.close()
        self._task = None

        if self._queue != []:
            location, callback = self._queue.pop()
            self._start_task(location, callback)


class DirectoryThumbnailer(WorkerThread):

    def __init__(self, app: 'FileViewApplication') -> None:
        worker = DirectoryThumbnailerWorker(app)
        super().__init__(worker)

    def request_thumbnail(self, location: 'Location', callback: ThumbnailCallback):
        print("request_thumbnail", location, callback)
        self._worker.sig_thumbnail_requested.emit(location, callback)


# EOF #
