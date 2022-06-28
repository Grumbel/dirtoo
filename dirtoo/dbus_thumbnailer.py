# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2017 Ingo Ruhnke <grumbel@gmail.com>
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

# https://wiki.gnome.org/action/show/DraftSpecs/ThumbnailerSpec


from typing import cast, Any, Dict, List, Tuple
from abc import ABC, abstractmethod

import logging
import os
import hashlib
import urllib.parse
import mimetypes
from enum import Enum
import xdg.BaseDirectory

from PyQt5.QtDBus import QDBus, QDBusReply, QDBusMessage, QDBusInterface
from PyQt5.QtCore import QObject, pyqtSlot, QVariant

logger = logging.getLogger(__name__)


class DBusThumbnailerError(Enum):

    # The URIs in failed_uris have (currently) unsupported URI schemes
    # or MIME types.
    UNSUPPORTED_MIMETYPE = 0

    # The connection to a specialized thumbnailer could not be
    # established properly.
    CONNECTION_FAILURE = 1

    # The URIs in failed_uris contain invalid image data or data that
    # does not match the MIME type.
    INVALID_DATA = 2

    # The URIs in failed_uris are thumbnail files themselves. We
    # disallow infinite recursion.
    THUMBNAIL_RECURSION = 3

    # The thumbnails for URIs in failed_uris could not be saved to the
    # disk.
    SAVE_FAILURE = 4

    # Unsupported flavor requested
    UNSUPPORTED_FLAVOR = 5


def dbus_as(value: List[str]) -> QVariant:
    var = QVariant(value)
    ret = var.convert(QVariant.StringList)
    assert ret, "QVariant conversion failure: {}".format(value)
    return var


def dbus_uint(value: int) -> QVariant:
    var = QVariant(value)
    ret = var.convert(QVariant.UInt)
    assert ret, "QVariant conversion failure: {}".format(value)
    return var


class DBusThumbnailerListener(ABC):

    @abstractmethod
    def started(self, handle: QVariant) -> None:
        pass

    @abstractmethod
    def ready(self, handle: QVariant, urls: List[str], flavor: str) -> None:
        pass

    @abstractmethod
    def error(self, handle: QVariant, failed_uris: List[str], error_code: 'DBusThumbnailerError', message: str) -> None:
        pass

    @abstractmethod
    def finished(self, handle: QVariant) -> None:
        pass

    @abstractmethod
    def idle(self) -> None:
        pass


class DBusThumbnailer(QObject):

    def __init__(self, bus: QDBus, listener: DBusThumbnailerListener) -> None:
        super().__init__()

        self.bus = bus

        self.bus.registerObject('/', self)
        self.requests: Dict[str, Tuple[List[str], List[str], str]] = {}
        self.thumbnailer = QDBusInterface(
            'org.freedesktop.thumbnails.Thumbnailer1',
            '/org/freedesktop/thumbnails/Thumbnailer1',
            'org.freedesktop.thumbnails.Thumbnailer1',
            connection=self.bus)
        self.listener = listener

        self.bus.connect('', '', 'org.freedesktop.thumbnails.Thumbnailer1',
                         'Ready', self._receive_ready)

        self.bus.connect('', '', 'org.freedesktop.thumbnails.Thumbnailer1',
                         'Started', self._receive_started)

        self.bus.connect('', '', 'org.freedesktop.thumbnails.Thumbnailer1',
                         'Finished', self._receive_finished)

        self.bus.connect('', '', 'org.freedesktop.thumbnails.Thumbnailer1',
                         'Error', self._receive_error)

    def close(self) -> None:
        self.bus.unregisterObject('/')

    def _add_request(self, handle: QVariant, data: Tuple[List[str], List[str], str]) -> None:
        self.requests[handle] = data

    def _remove_request(self, handle: QVariant) -> None:
        if handle in self.requests:
            del self.requests[handle]

        if not self.requests:
            self.listener.idle()

    @pyqtSlot(QDBusMessage)  # type: ignore
    def _receive_started(self, msg: QDBusMessage) -> None:
        handle, = msg.arguments()
        self.listener.started(handle)

    @pyqtSlot(QDBusMessage)  # type: ignore
    def _receive_ready(self, msg: QDBusMessage) -> None:
        handle, uris = msg.arguments()
        data = self.requests[handle]
        self.listener.ready(handle, uris, data[2])

    @pyqtSlot(QDBusMessage)  # type: ignore
    def _receive_finished(self, msg: QDBusMessage) -> None:
        handle, = msg.arguments()
        self.listener.finished(handle)
        self._remove_request(handle)

    @pyqtSlot(QDBusMessage)  # type: ignore
    def _receive_error(self, msg: QDBusMessage) -> None:
        handle, failed_uris, error_code, message = msg.arguments()
        self.listener.error(handle, failed_uris, error_code, message)

    def _call(self, method: str, *args: Any) -> List[QVariant]:
        msg = self.thumbnailer.call(method, *args)
        reply = QDBusReply(msg)
        if not reply.isValid():
            raise Exception("Error on method call '{}': {}: {}".format(
                method,
                reply.error().name(),
                reply.error().message()))

        return cast(List[QVariant], msg.arguments())

    def queue(self, files: List[str], flavor: str = "default") -> QVariant:
        logger.debug("DBusThumbnailer.queue: %s  %s", files, flavor)

        if files == []:
            return None

        urls = ["file://" + urllib.parse.quote(os.path.abspath(f)) for f in files]
        mime_types = [
            mimetypes.guess_type(url)[0] or "application/octet-stream"
            for url in urls
        ]

        handle, = self._call(
            "Queue",
            dbus_as(urls),  # uris: as
            dbus_as(mime_types),  # mime_types: as
            flavor,  # flavor: s
            "foreground",  # scheduler: s
            dbus_uint(0),  # handle_to_dequeue: u
            # <arg type="u" name="handle" direction="out" />
        )

        self._add_request(handle, (urls, mime_types, flavor))
        return handle

    def dequeue(self, handle: QVariant) -> None:
        logger.debug("DBusThumbnailer.dequeue: %s", handle)

        handle, = self._call("Dequeue", handle)
        del self.requests[handle]

    def get_supported(self) -> Tuple[str, str]:
        uri_schemes, mime_types = self._call("GetSupported")
        return (uri_schemes, mime_types)

    def get_schedulers(self) -> List[str]:
        schedulers = self._call("GetSchedulers")[0]
        return cast(List[str], schedulers)

    def get_flavors(self) -> List[str]:
        flavors, = self._call("GetFlavors")
        return cast(List[str], flavors)

    @staticmethod
    def thumbnail_from_filename(filename: str, flavor: str = "normal") -> str:
        url = "file://" + urllib.parse.quote(os.path.abspath(filename))
        digest = hashlib.md5(os.fsencode(url)).hexdigest()
        result = os.path.join(xdg.BaseDirectory.xdg_cache_home, "thumbnails", flavor, digest + ".png")
        return result

    @staticmethod
    def thumbnail_from_url(url: str, flavor: str = "normal") -> str:
        digest = hashlib.md5(os.fsencode(url)).hexdigest()
        result = os.path.join(xdg.BaseDirectory.xdg_cache_home, "thumbnails", flavor, digest + ".png")
        return result


# EOF #
