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
import dbus
from typing import List, Callable, Dict, Tuple, Union
from dbus.mainloop.pyqt5 import DBusQtMainLoop

from PyQt5.QtGui import QPixmap, QImage
from PyQt5.QtCore import (QObject, pyqtSignal, QThread,
                          QAbstractEventDispatcher, QEventLoop)

from dirtools.dbus_thumbnailer import DBusThumbnailer


ThumbnailCallback = Callable[[str, Union[str, None], QPixmap], None]


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

    # Emitted when the worker as been shut down completely
    sig_finished = pyqtSignal()

    sig_thumbnail_ready = pyqtSignal(str, str, CallableWrapper, QImage)
    sig_thumbnail_error = pyqtSignal(str, str, CallableWrapper)

    def __init__(self):
        super().__init__()
        # This function is called from the main thread, leave
        # construction to init() and deinit()
        self.quit = False

        self.dbus_loop = None
        self.session_bus = None
        self.dbus_thumbnailer: Union[DBusThumbnailer, None] = None
        self.requests: Dict[int, List[Tuple[str, str, ThumbnailCallback]]] = {}

    def init(self):
        self.dbus_loop = DBusQtMainLoop(set_as_default=False)
        self.session_bus = dbus.SessionBus(mainloop=self.dbus_loop)
        self.dbus_thumbnailer = DBusThumbnailer(self.session_bus,
                                                WorkerDBusThumbnailerListener(self))

    def deinit(self):
        assert self.quit

        self.session_bus.close()
        del self.dbus_thumbnailer
        del self.session_bus
        del self.dbus_loop

        self.sig_finished.emit()

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
            self.sig_thumbnail_error.emit(filename, flavor, callback)


class Thumbnailer(QObject):

    sig_thumbnail_requested = pyqtSignal(str, str, CallableWrapper)
    sig_thumbnail_error = pyqtSignal(str, str, CallableWrapper)

    sig_close_requested = pyqtSignal()

    def __init__(self):
        super().__init__()

        self.worker = ThumbnailerWorker()
        self.thread = QThread()
        self.worker.moveToThread(self.thread)

        # startup and shutdown
        self.thread.started.connect(self.worker.init)
        self.thread.finished.connect(self.thread.deleteLater)
        self.sig_close_requested.connect(self.worker.deinit)
        self.worker.sig_finished.connect(self.thread.quit)
        self.worker.sig_finished.connect(self.deleteLater)

        # requests to the worker
        self.sig_thumbnail_requested.connect(self.worker.on_thumbnail_requested)

        # replies from the worker
        self.worker.sig_thumbnail_ready.connect(self.on_thumbnail_ready)
        self.worker.sig_thumbnail_error.connect(self.on_thumbnail_error)

        self.thread.start()

    def close(self):

        # This signal won't be received until the worker is idle, so
        # set .quit manually for a fast exit
        self.worker.quit = True
        self.sig_close_requested.emit()

        # waiting for the thread to finish
        event_dispatcher = QAbstractEventDispatcher.instance()
        while not self.thread.wait(10):
            event_dispatcher.processEvents(QEventLoop.AllEvents)

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
            callback(filename, flavor, None)
        else:
            callback(filename, flavor, pixmap)

    def on_thumbnail_error(self, filename: str, flavor: str,
                           callback: ThumbnailCallback):
        callback(filename, flavor, None)


# EOF #
