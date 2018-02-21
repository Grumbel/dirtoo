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


import os
from typing import List, Callable, Dict, Tuple, Optional

from PyQt5.QtCore import Qt, QObject, pyqtSignal, QThread
from PyQt5.QtDBus import QDBusConnection
from PyQt5.QtGui import QPixmap, QImage

from dirtools.dbus_thumbnailer import DBusThumbnailer


ThumbnailCallback = Callable[[str, Optional[str], QPixmap, int, str], None]


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

    sig_thumbnail_ready = pyqtSignal(str, str, CallableWrapper, QImage)
    sig_thumbnail_error = pyqtSignal(str, str, CallableWrapper, int, str)

    def __init__(self, parent=None):
        super().__init__(parent)
        # This function is called from the main thread, leave
        # construction to init() and deinit()
        self._close = False

        self.dbus_loop = None
        self.session_bus = None
        self.dbus_thumbnailer: Optional[DBusThumbnailer] = None
        self.requests: Dict[int, List[Tuple[str, str, ThumbnailCallback]]] = {}

    def init(self):
        self.session_bus = QDBusConnection.sessionBus()
        self.dbus_thumbnailer = DBusThumbnailer(self.session_bus,
                                                WorkerDBusThumbnailerListener(self))

    def close(self):
        assert self._close

        del self.dbus_thumbnailer

    def on_thumbnail_requested(self, filename: str, flavor: str,
                               callback: ThumbnailCallback):
        thumbnail_filename = DBusThumbnailer.thumbnail_from_filename(filename, flavor)
        if os.path.exists(thumbnail_filename):
            image = QImage(thumbnail_filename)
            self.sig_thumbnail_ready.emit(filename, flavor, callback, image)
        else:
            handle = self.dbus_thumbnailer.queue([filename], flavor)

            if handle not in self.requests:
                self.requests[handle] = []

            self.requests[handle].append((filename, flavor, callback))

    def on_thumbnail_started(self, handle: int):
        pass

    def on_thumbnail_finished(self, handle: int):
        del self.requests[handle]

    def on_thumbnail_ready(self, handle: int, urls: List[str], flavor: str):
        for request in self.requests.get(handle, []):
            filename, flavor, callback = request

            thumbnail_filename = DBusThumbnailer.thumbnail_from_filename(filename, flavor)

            # On shutdown this causes:
            # QObject::~QObject: Timers cannot be stopped from another thread
            image = QImage(thumbnail_filename)

            self.sig_thumbnail_ready.emit(filename, flavor, callback, image)

    def on_thumbnail_error(self, handle: int, urls: List[str], error_code, message):
        for request in self.requests.get(handle, []):
            filename, flavor, callback = request
            self.sig_thumbnail_error.emit(filename, flavor, callback, error_code, message)


class Thumbnailer(QObject):

    sig_thumbnail_requested = pyqtSignal(str, str, CallableWrapper)
    sig_thumbnail_error = pyqtSignal(str, str, CallableWrapper)

    sig_close_requested = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)

        self.worker = ThumbnailerWorker()
        self.thread = QThread(self)
        self.worker.moveToThread(self.thread)

        # startup and shutdown
        self.thread.started.connect(self.worker.init)
        self.sig_close_requested.connect(self.worker.close, type=Qt.BlockingQueuedConnection)

        # requests to the worker
        self.sig_thumbnail_requested.connect(self.worker.on_thumbnail_requested)

        # replies from the worker
        self.worker.sig_thumbnail_ready.connect(self.on_thumbnail_ready)
        self.worker.sig_thumbnail_error.connect(self.on_thumbnail_error)

        self.thread.start()

    def close(self):
        self.worker._close = True
        self.sig_close_requested.emit()
        self.thread.quit()
        self.thread.wait()

    def request_thumbnail(self, filename: str, flavor: str,
                          callback: ThumbnailCallback):
        self.sig_thumbnail_requested.emit(filename, flavor, CallableWrapper(callback))

    def delete_thumbnails(self, files: List[str]):
        print("Thumbnailer.delete_thumbnails: not implemented")

    def on_thumbnail_ready(self, filename: str, flavor: str,
                           callback: ThumbnailCallback,
                           image: QImage):
        pixmap = QPixmap(image)
        if pixmap.isNull():
            callback(filename, flavor, None, None, None)
        else:
            callback(filename, flavor, pixmap, None, None)

    def on_thumbnail_error(self, filename: str, flavor: str,
                           callback: ThumbnailCallback,
                           error_code, message):
        callback(filename, flavor, None, error_code, message)


# EOF #
