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
import os
from typing import List, Callable, Dict, Tuple, Optional

from PyQt5.QtCore import Qt, QObject, pyqtSignal, QThread
from PyQt5.QtDBus import QDBusConnection
from PyQt5.QtGui import QPixmap, QImage

from dirtools.dbus_thumbnailer import DBusThumbnailer
from dirtools.fileview.location import Location

logger = logging.getLogger(__name__)


ThumbnailCallback = Callable[[Location, Optional[str], QPixmap, int, str], None]


class CallableWrapper:

    def __init__(self, func):
        self.func = func

    def __call__(self, *args):
        return self.func(*args)


class WorkerDBusThumbnailerListener:

    def __init__(self, worker):
        self.worker = worker

    def started(self, handle):
        self.worker.on_thumbnail_started(handle)

    def ready(self, handle, urls, flavor):
        self.worker.on_thumbnail_ready(handle, urls, flavor)

    def error(self, handle, uris, error_code, message):
        self.worker.on_thumbnail_error(handle, uris, error_code, message)

    def finished(self, handle):
        self.worker.on_thumbnail_finished(handle)

    def idle(self):
        pass


class ThumbnailerWorker(QObject):

    sig_thumbnail_ready = pyqtSignal(Location, str, CallableWrapper, QImage)
    sig_thumbnail_error = pyqtSignal(Location, str, CallableWrapper, int, str)

    def __init__(self, vfs, parent=None):
        super().__init__(parent)
        # This function is called from the main thread, leave
        # construction to init() and deinit()

        self.vfs = vfs
        self._close = False

        self.dbus_loop = None
        self.session_bus = None
        self.dbus_thumbnailer: Optional[DBusThumbnailer] = None
        self.requests: Dict[int, List[Tuple[Location, str, ThumbnailCallback]]] = {}

    def init(self):
        self.session_bus = QDBusConnection.sessionBus()
        self.dbus_thumbnailer = DBusThumbnailer(self.session_bus,
                                                WorkerDBusThumbnailerListener(self))

    def close(self):
        assert self._close

        del self.dbus_thumbnailer

    def on_thumbnail_requested(self, location: Location, flavor: str,
                               callback: ThumbnailCallback):
        thumbnail_filename = DBusThumbnailer.thumbnail_from_filename(self.vfs.get_stdio_name(location), flavor)
        if os.path.exists(thumbnail_filename):
            image = QImage(thumbnail_filename)
            self.sig_thumbnail_ready.emit(location, flavor, callback, image)
        else:
            filename = self.vfs.get_stdio_name(location)
            handle = self.dbus_thumbnailer.queue([filename], flavor)

            if handle not in self.requests:
                self.requests[handle] = []

            self.requests[handle].append((location, flavor, callback))

    def on_thumbnail_started(self, handle: int):
        pass

    def on_thumbnail_finished(self, handle: int):
        del self.requests[handle]

    def on_thumbnail_ready(self, handle: int, urls: List[str], flavor: str):
        for request in self.requests.get(handle, []):
            location, flavor, callback = request

            thumbnail_filename = DBusThumbnailer.thumbnail_from_filename(
                self.vfs.get_stdio_name(location), flavor)

            # On shutdown this causes:
            # QObject::~QObject: Timers cannot be stopped from another thread
            image = QImage(thumbnail_filename)

            self.sig_thumbnail_ready.emit(location, flavor, callback, image)

    def on_thumbnail_error(self, handle: int, urls: List[str], error_code, message):
        for request in self.requests.get(handle, []):
            location, flavor, callback = request
            self.sig_thumbnail_error.emit(location, flavor, callback, error_code, message)


class Thumbnailer(QObject):

    sig_thumbnail_requested = pyqtSignal(Location, str, CallableWrapper)
    sig_thumbnail_error = pyqtSignal(Location, str, CallableWrapper)

    sig_close_requested = pyqtSignal()

    def __init__(self, vfs, parent=None):
        super().__init__(parent)

        self._worker = ThumbnailerWorker(vfs)
        self._thread = QThread(self)
        self._worker.moveToThread(self._thread)

        # startup and shutdown
        self._thread.started.connect(self._worker.init)
        self.sig_close_requested.connect(self._worker.close, type=Qt.BlockingQueuedConnection)

        # requests to the worker
        self.sig_thumbnail_requested.connect(self._worker.on_thumbnail_requested)

        # replies from the worker
        self._worker.sig_thumbnail_ready.connect(self.on_thumbnail_ready)
        self._worker.sig_thumbnail_error.connect(self.on_thumbnail_error)

        self._thread.start()

    def close(self):
        self._worker._close = True
        self.sig_close_requested.emit()
        self._thread.quit()
        self._thread.wait()

    def request_thumbnail(self, location: Location, flavor: str,
                          callback: ThumbnailCallback):
        logger.debug("Thumbnailer.request_thumbnail: %s  %s", location, flavor)
        self.sig_thumbnail_requested.emit(location, flavor, CallableWrapper(callback))

    def delete_thumbnails(self, files: List[str]):
        logger.warning("Thumbnailer.delete_thumbnail (not implemented): %s", files)

    def on_thumbnail_ready(self, location: Location, flavor: str,
                           callback: ThumbnailCallback,
                           image: QImage):
        pixmap = QPixmap(image)
        if pixmap.isNull():
            callback(location, flavor, None, None, None)
        else:
            callback(location, flavor, pixmap, None, None)

    def on_thumbnail_error(self, location: Location, flavor: str,
                           callback: ThumbnailCallback,
                           error_code, message):
        callback(location, flavor, None, error_code, message)


# EOF #
