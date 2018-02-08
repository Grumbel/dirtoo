# dirtool.py - diff tool for directories
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


from typing import Dict, Tuple

import os
import hashlib
import urllib.parse
import mimetypes
import xdg.BaseDirectory

from PyQt5.QtDBus import QDBusReply, QDBusMessage, QDBusInterface
from PyQt5.QtCore import QObject, pyqtSlot, QVariant


def dbus_as(value):
    var = QVariant(value)
    ret = var.convert(QVariant.StringList)
    assert ret, "QVariant conversion failure: %s".format(value)
    return var


def dbus_uint(value):
    var = QVariant(value)
    ret = var.convert(QVariant.UInt)
    assert ret, "QVariant conversion failure: %s".format(value)
    return var


class DBusThumbnailerListener:

    def __init__(self):
        pass

    def started(self, handle):
        pass

    def ready(self, handle, urls, flavor):
        pass

    def error(self, handle, failed_uris, error_code, message):
        pass

    def finished(self, handle):
        pass

    def idle(self):
        pass


class DBusThumbnailer(QObject):

    def __init__(self, bus, listener=None):
        super().__init__()

        self.bus = bus

        self.bus.registerObject('/', self)
        self.requests: Dict[str, Tuple[str, str, str]] = {}
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

    def close(self):
        self.bus.unregisterObject('/')

    def _add_request(self, handle, data):
        self.requests[handle] = data

    def _remove_request(self, handle):
        if handle in self.requests:
            del self.requests[handle]

        if not self.requests:
            self.listener.idle()

    @pyqtSlot(QDBusMessage)
    def _receive_started(self, msg):
        handle, = msg.arguments()
        self.listener.started(handle)

    @pyqtSlot(QDBusMessage)
    def _receive_ready(self, msg):
        handle, uris = msg.arguments()
        data = self.requests[handle]
        self.listener.ready(handle, uris, data[2])

    @pyqtSlot(QDBusMessage)
    def _receive_finished(self, msg):
        handle, = msg.arguments()
        self.listener.finished(handle)
        self._remove_request(handle)

    @pyqtSlot(QDBusMessage)
    def _receive_error(self, msg):
        handle, failed_uris, error_code, message = msg.arguments()
        self.listener.error(handle, failed_uris, error_code, message)

    def _call(self, method, *args):
        msg = self.thumbnailer.call(method, *args)
        reply = QDBusReply(msg)
        if not reply.isValid():
            raise Exception("Error on method call '{}': {}: {}".format(
                method,
                reply.error().name(),
                reply.error().message()))
        else:
            return msg.arguments()

    def queue(self, files, flavor="default"):
        if files == []:
            return
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
            "default",  # scheduler: s
            dbus_uint(0),  # handle_to_dequeue: u
            # <arg type="u" name="handle" direction="out" />
        )

        self._add_request(handle, (urls, mime_types, flavor))
        return handle

    def dequeue(self, handle):
        handle, = self._call("Dequeue", handle)
        del self.requests[handle]

    def get_supported(self):
        uri_schemes, mime_types = self._call("GetSupported")
        return (uri_schemes, mime_types)

    def get_schedulers(self):
        schedulers, = self._call("GetSchedulers")
        return schedulers

    def get_flavors(self):
        flavors, = self._call("GetFlavors")
        return flavors

    @staticmethod
    def thumbnail_from_filename(filename, flavor="normal"):
        url = "file://" + urllib.parse.quote(os.path.abspath(filename))
        digest = hashlib.md5(os.fsencode(url)).hexdigest()
        result = os.path.join(xdg.BaseDirectory.xdg_cache_home, "thumbnails", flavor, digest + ".png")
        return result

    @staticmethod
    def thumbnail_from_url(url, flavor="normal"):
        digest = hashlib.md5(os.fsencode(url)).hexdigest()
        result = os.path.join(xdg.BaseDirectory.xdg_cache_home, "thumbnails", flavor, digest + ".png")
        return result


# EOF #