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

from PyQt5.QtDBus import QDBusReply, QDBusInterface
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
        self.requests: Dict[str, Tuple[str, str, str]] = {}
        self.thumbnailer = QDBusInterface(
            'org.freedesktop.thumbnails.Thumbnailer1',
            '/org/freedesktop/thumbnails/Thumbnailer1',
            'org.freedesktop.thumbnails.Thumbnailer1',
            connection=bus)
        self.listener = listener
        bus.registerObject('/', self)
        bus.connect('org.freedesktop.thumbnails.Thumbnailer1',
                    '/org/freedesktop/thumbnails/Thumbnailer1',
                    'org.freedesktop.thumbnails.Thumbnailer1',
                    'Ready', self._receive_ready)

        bus.connect('org.freedesktop.thumbnails.Thumbnailer1',
                    '/org/freedesktop/thumbnails/Thumbnailer1',
                    'org.freedesktop.thumbnails.Thumbnailer1',
                    'Started', self._receive_started)

        bus.connect('org.freedesktop.thumbnails.Thumbnailer1',
                    '/org/freedesktop/thumbnails/Thumbnailer1',
                    'org.freedesktop.thumbnails.Thumbnailer1',
                    'Finished', self._receive_finished)

        bus.connect('org.freedesktop.thumbnails.Thumbnailer1',
                    '/org/freedesktop/thumbnails/Thumbnailer1',
                    'org.freedesktop.thumbnails.Thumbnailer1',
                    'Error', self._receive_error)

    def _add_request(self, handle, data):
        self.requests[handle] = data

    def _remove_request(self, handle):
        if handle in self.requests:
            del self.requests[handle]

        if not self.requests:
            self.listener.idle()

    @pyqtSlot(QVariant, QVariant)
    def _receive_started(self, handle):
        print("started")
        self.listener.started(handle)

    @pyqtSlot(QVariant, QVariant)
    def _receive_ready(self, handle, uris):
        print("ready")
        data = self.requests[handle]
        self.listener.ready(handle, uris, data[2])

    @pyqtSlot(QVariant, QVariant)
    def _receive_finished(self, handle):
        print("finished")
        self.listener.finished(handle)
        self._remove_request(handle)

    @pyqtSlot(QVariant, QVariant)
    def _receive_error(self, handle, failed_uris, error_code, message):
        print("error")
        self.listener.error(handle, failed_uris, error_code, message)

    def queue(self, files, flavor="default"):
        if files == []:
            return
        print("ueue", files)
        urls = ["file://" + urllib.parse.quote(os.path.abspath(f)) for f in files]
        mime_types = [
            mimetypes.guess_type(url)[0] or "application/octet-stream"
            for url in urls
        ]

        msg = self.thumbnailer.call(
            "Queue",
            dbus_as(urls),  # uris: as
            dbus_as(mime_types),  # mime_types: as
            flavor,  # flavor: s
            "default",  # scheduler: s
            dbus_uint(0),  # handle_to_dequeue: u
            # <arg type="u" name="handle" direction="out" />
        )
        reply = QDBusReply(msg)

        print("OK", reply.error().message())
        handle = reply.value()
        self._add_request(handle, (urls, mime_types, flavor))

    def dequeue(self, handle):
        msg = self.thumbnailer.call("Dequeue", handle)
        reply = QDBusReply(msg)
        if not reply.isValid():
            err = reply.error()
            print("{}: {}".format(err.name(), err.message()))
        del self.requests[handle]

    def get_supported(self):
        reply = self.thumbnailer.call("GetSupported")
        uri_schemes, mime_types = reply.arguments()
        return (uri_schemes, mime_types)

    def get_schedulers(self):
        schedulers = QDBusReply(self.thumbnailer.call("GetSchedulers")).value()
        return schedulers

    def get_flavors(self):
        flavors = QDBusReply(self.thumbnailer.call("GetFlavors")).value()
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
