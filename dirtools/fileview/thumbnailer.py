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


from typing import List, Callable, Dict, Optional

import logging
import os
from collections import defaultdict, namedtuple

from PyQt5.QtCore import Qt, QObject, pyqtSignal, QThread
from PyQt5.QtDBus import QDBusConnection
from PyQt5.QtGui import QImage

from dirtools.dbus_thumbnailer import DBusThumbnailer
from dirtools.fileview.location import Location

logger = logging.getLogger(__name__)


ThumbnailCallback = Callable[[Location, Optional[str], QImage, int, str], None]


class WorkerDBusThumbnailerListener:

    def __init__(self, worker):
        self._worker = worker

    def started(self, handle):
        self._worker.on_thumbnail_started(handle)

    def ready(self, handle, urls, flavor):
        self._worker.on_thumbnail_ready(handle, urls, flavor)

    def error(self, handle, uris, error_code, message):
        self._worker.on_thumbnail_error(handle, uris, error_code, message)

    def finished(self, handle):
        self._worker.on_thumbnail_finished(handle)

    def idle(self):
        pass


ThumbnailRequest = namedtuple('ThumbnailRequest', ['location', 'flavor', 'callback'])


class ThumbnailerWorker(QObject):

    # location, flavor, callback, image
    sig_thumbnail_ready = pyqtSignal(Location, str, object, QImage)

    # location, flavor, callback, error_code, error_message
    sig_thumbnail_error = pyqtSignal(Location, str, object, int, str)

    def __init__(self, vfs, parent=None) -> None:
        super().__init__(parent)
        # This function is called from the main thread, leave
        # construction to init() and deinit()

        self._vfs = vfs
        self._close = False

        self._dbus_thumbnailer: Optional[DBusThumbnailer] = None
        self._queued_requests: Dict[int, List[ThumbnailRequest]] = defaultdict(list)

        self._timer_id = 0
        self._thumbnail_requests: List[ThumbnailRequest] = []

    def init(self):
        self._dbus_thumbnailer = DBusThumbnailer(QDBusConnection.sessionBus(),
                                                 WorkerDBusThumbnailerListener(self))

    def close(self):
        assert self._close

        del self._dbus_thumbnailer

    def timerEvent(self, ev) -> None:
        req_by_flavor: defaultdict = defaultdict(list)
        for req in self._thumbnail_requests:
            req_by_flavor[req.flavor].append(req)

        for flavor, reqs in req_by_flavor.items():
            logger.debug("Thumbnailer: requesting a batch of %s thubnails", len(reqs))

            filenames = []
            for req in reqs:
                filenames.append(self._vfs.get_stdio_name(req.location))

            handle = self._dbus_thumbnailer.queue(filenames, flavor)
            self._queued_requests[handle].append(reqs)

        self._thumbnail_requests.clear()

    def on_thumbnail_requested(self, location: Location, flavor: str, force: bool,
                               callback: ThumbnailCallback):

        thumbnail_filename = DBusThumbnailer.thumbnail_from_url(location.as_url(), flavor)
        exists = os.path.exists(thumbnail_filename)
        if exists and not force:
            image = QImage(thumbnail_filename)
            self.sig_thumbnail_ready.emit(location, flavor, callback, image)
        else:
            if exists and force:
                # DBusThumbnailCache.delete doesn't seem to be able to
                # get the file deleted fast enough. The request thus
                # isn't guranteed to regenerate the thumbnail. So
                # employ brute force to get rid of the thumbnail
                # quickly.
                #
                # self.dbus_thumbnail_cache.delete(location.as_url())
                os.unlink(thumbnail_filename)

            self._thumbnail_requests.append(ThumbnailRequest(location, flavor, callback))

            if self._timer_id == 0:
                self._timer_id = self.startTimer(500)

    def on_thumbnail_started(self, handle: int):
        pass

    def on_thumbnail_finished(self, handle: int):
        del self._queued_requests[handle]

    def _find_requests(self, handle, urls) -> List[ThumbnailRequest]:
        results = []
        for reqs in self._queued_requests.get(handle, []):
            for req in reqs:
                req_url = self._vfs.get_stdio_url(req.location)
                if req_url in urls:
                    results.append(req)
        return results

    def on_thumbnail_ready(self, handle: int, urls: List[str], flavor: str):
        reqs = self._find_requests(handle, urls)

        for req in reqs:
            thumbnail_filename = DBusThumbnailer.thumbnail_from_filename(
                self._vfs.get_stdio_name(req.location), req.flavor)

            # On shutdown this causes:
            # QObject::~QObject: Timers cannot be stopped from another thread
            image = QImage(thumbnail_filename)

            self.sig_thumbnail_ready.emit(req.location, req.flavor, req.callback, image)

    def on_thumbnail_error(self, handle: int, urls: List[str], error_code, message):
        reqs = self._find_requests(handle, urls)
        for req in reqs:
            self.sig_thumbnail_error.emit(req.location, req.flavor, req.callback, error_code, message)


class Thumbnailer(QObject):

    # location, flavor, force, callback
    sig_thumbnail_requested = pyqtSignal(Location, str, bool, object)

    # location, flavor, callback
    sig_thumbnail_error = pyqtSignal(Location, str, object)

    sig_close_requested = pyqtSignal()

    def __init__(self, vfs, parent=None) -> None:
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

    def request_thumbnail(self, location: Location, flavor: str, force: bool,
                          callback: ThumbnailCallback):
        logger.debug("Thumbnailer.request_thumbnail: %s  %s", location, flavor)
        self.sig_thumbnail_requested.emit(location, flavor, force, callback)

    def delete_thumbnails(self, files: List[str]):
        logger.warning("Thumbnailer.delete_thumbnail (not implemented): %s", files)

    def on_thumbnail_ready(self, location: Location, flavor: str,
                           callback: ThumbnailCallback,
                           image: QImage):
        if image.isNull():
            callback(location, flavor, None, None, None)
        else:
            callback(location, flavor, image, None, None)

    def on_thumbnail_error(self, location: Location, flavor: str,
                           callback: ThumbnailCallback,
                           error_code, message):
        callback(location, flavor, None, error_code, message)


# EOF #
