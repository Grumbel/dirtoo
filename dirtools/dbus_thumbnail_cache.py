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
import urllib.parse

from PyQt5.Qt import QVariant
from PyQt5.QtDBus import QDBusInterface, QDBusReply


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


def url_from_path(path):
    return "file://{}".format(urllib.parse.quote(os.path.abspath(path)))


class DBusThumbnailCache:

    def __init__(self, bus):
        self.cache = QDBusInterface(
            'org.freedesktop.thumbnails.Cache1',
            '/org/freedesktop/thumbnails/Cache1',
            'org.freedesktop.thumbnails.Cache1',
            bus)

    def _call(self, method, *args):
        msg = self.cache.call(method, *args)
        reply = QDBusReply(msg)
        if not reply.isValid():
            raise Exception("Error on method call '{}': {}: {}".format(
                method,
                reply.error().name(),
                reply.error().message()))
        else:
            return msg.arguments()

    def delete(self, files):
        urls = [url_from_path(f) for f in files]
        self._call("Delete", dbus_as(urls))

    def cleanup(self, files, mtime_threshold=0):
        urls = ["file://" + urllib.parse.quote(os.path.abspath(f)) for f in files]
        self._call("Cleanup", dbus_as(urls), mtime_threshold)

    def copy(self, from_files, to_files):
        from_uris = [url_from_path(f) for f in from_files]
        to_uris = [url_from_path(f) for f in to_files]
        self._call("Copy", dbus_as(from_uris), dbus_as(to_uris))

    def move(self, from_files, to_files):
        from_uris = [url_from_path(f) for f in from_files]
        to_uris = [url_from_path(f) for f in to_files]
        self._call("Move", dbus_as(from_uris), dbus_as(to_uris))


# EOF #
