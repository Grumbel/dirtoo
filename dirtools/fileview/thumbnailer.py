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


import dbus
import types
from typing import List, Callable
from dbus.mainloop.pyqt5 import DBusQtMainLoop
import urllib.parse

from PyQt5.QtGui import QPixmap
from PyQt5.QtCore import (QObject, pyqtSignal, QThread,
                          QAbstractEventDispatcher, QEventLoop)

from dirtools.dbus_thumbnailer import DBusThumbnailer


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

    sig_thumbnail_ready = pyqtSignal(str, types.FunctionType, QPixmap)
    sig_thumbnail_error = pyqtSignal(str, types.FunctionType)

    def __init__(self):
        print(QThread.currentThread(), "ThumbnailerWorker.__init__")
        super().__init__()
        # This function is called from the main thread, leave
        # construction to init() and deinit()
        self.quit = False

        self.dbus_loop = None
        self.session_bus = None
        self.dbus_thumbnailer = None
        self.thumbnailer_cache = None
        self.requests = {}

    def init(self):
        print(QThread.currentThread(), "ThumbnailerWorker.init")
        print("ThumbnailerWorker.init")
        self.dbus_loop = DBusQtMainLoop(set_as_default=False)
        self.session_bus = dbus.SessionBus(mainloop=self.dbus_loop)
        self.dbus_thumbnailer = DBusThumbnailer(self.session_bus,
                                                WorkerDBusThumbnailerListener(self))

    def deinit(self):
        print(QThread.currentThread(), "ThumbnailerWorker.deinit")
        print("ThumbnailerWorker.deinit")
        assert self.quit

        del self.thumbnailer_cache
        self.session_bus.close()
        del self.dbus_thumbnailer
        del self.session_bus
        del self.dbus_loop

        print("---worker Finished--")
        self.sig_finished.emit()

    def on_thumbnail_requested(self, filename: str, flavor: str,
                               callback: types.FunctionType):
        print(QThread.currentThread(), "ThumbnailerWorker.on_thumbnail_requested")
        print("-------------Request started:")
        handle = self.dbus_thumbnailer.queue([filename], flavor)
        print("-------------Request done:", handle)

        if handle not in self.requests:
            self.requests[handle] = []

        self.requests[handle].append((filename, callback))

    def on_thumbnail_started(self, handle: int):
        print(QThread.currentThread(), "ThumbnailerWorker.on_thumbnail_started")
        pass

    def on_thumbnail_finished(self, handle: int):
        print(QThread.currentThread(), "ThumbnailerWorker.on_thumbnail_finished")
        print("finished: Requests", self.requests.items())
        del self.requests[handle]

    def on_thumbnail_ready(self, handle: int, urls: List[str], flavor: str):
        print(QThread.currentThread(), "ThumbnailerWorker.on_thumbnail_ready")
        for request in self.requests.get(handle, []):
            filename, callback = request

            thumbnail_filename = DBusThumbnailer.thumbnail_from_filename(filename, flavor)

            # On shutdown this causes:
            # QObject::~QObject: Timers cannot be stopped from another thread
            pixmap = QPixmap(thumbnail_filename)

            self.sig_thumbnail_ready.emit(filename, callback, pixmap)

    def on_thumbnail_error(self, handle: int, urls: List[str], error_code, message):
        print(QThread.currentThread(), "ThumbnailerWorker.on_thumbnail_error")
        print(handle, urls, error_code, message)
        for request in self.requests.get(handle, []):
            filename, callback = request
            self.sig_thumbnail_error.emit(filename, callback)


class Thumbnailer(QObject):

    sig_thumbnail_requested = pyqtSignal(str, str, types.FunctionType)
    sig_thumbnail_error = pyqtSignal(str, types.FunctionType)

    sig_close_requested = pyqtSignal()

    def __init__(self):
        print(QThread.currentThread(), "Thumbnailer.__init__")
        super().__init__()

        self.worker = ThumbnailerWorker()
        self.thread = QThread()
        self.worker.moveToThread(self.thread)

        # startup and shutdown
        self.thread.started.connect(self.worker.init)
        self.sig_close_requested.connect(self.worker.deinit)
        self.worker.sig_finished.connect(self.thread.quit)

        # requests to the worker
        self.sig_thumbnail_requested.connect(self.worker.on_thumbnail_requested)

        # replies from the worker
        self.worker.sig_thumbnail_ready.connect(self.on_thumbnail_ready)
        self.worker.sig_thumbnail_error.connect(self.on_thumbnail_error)

        self.thread.start()

    def close(self):
        print(QThread.currentThread(), "Thumbnailer.__close__")

        # This signal won't be received until the worker is idle
        self.sig_close_requested.emit()

        # Thus notify worker via sidechannel to stop processing it's jobs
        self.worker.quit = True

        print("evdi")
        # waiting for the thread to finish
        event_dispatcher = QAbstractEventDispatcher.instance()
        while self.thread.wait(10) == False:
            event_dispatcher.processEvents(QEventLoop.AllEvents)
        print("evdi:done")

        del self.thread
        del self.worker

    def request_thumbnail(self,
                          filename: str,
                          flavor: str,
                          callback: Callable):
        print(QThread.currentThread(), "Thumbnailer.request_thumbnail")
        self.sig_thumbnail_requested.emit(filename, flavor, callback)

    def on_thumbnail_ready(self,
                           filename: str,
                           callback: Callable,
                           pixmap: QPixmap):
        print(QThread.currentThread(), "Thumbnailer.on_thumbnail_ready")
        callback(filename, pixmap)

    def on_thumbnail_error(self,
                           filename: str,
                           callback: Callable):
        print(QThread.currentThread(), "Thumbnailer.on_thumbnail_error")
        callback(filename, None)


# EOF #
