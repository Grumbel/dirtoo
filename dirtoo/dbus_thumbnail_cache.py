# dirtoo - File and directory manipulation tools for Python
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


from typing import cast, List

import os
import urllib.parse

from PyQt5.Qt import QVariant
from PyQt5.QtDBus import QDBusInterface, QDBusReply


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


def url_from_path(path: str) -> str:
    return "file://{}".format(urllib.parse.quote(os.path.abspath(path)))


class DBusThumbnailCache:

    def __init__(self, bus: str) -> None:
        self.cache = QDBusInterface(
            'org.freedesktop.thumbnails.Cache1',
            '/org/freedesktop/thumbnails/Cache1',
            'org.freedesktop.thumbnails.Cache1',
            bus)

    def _call(self, method: str, *args: QVariant) -> List[QVariant]:
        msg = self.cache.call(method, *args)
        reply = QDBusReply(msg)
        if not reply.isValid():
            raise Exception("Error on method call '{}': {}: {}".format(
                method,
                reply.error().name(),
                reply.error().message()))

        return cast(List[QVariant], msg.arguments())

    def delete(self, files: List[str]) -> None:
        urls = [url_from_path(f) for f in files]
        self._call("Delete", dbus_as(urls))

    def cleanup(self, files: List[str], mtime_threshold: int = 0) -> None:
        urls = ["file://" + urllib.parse.quote(os.path.abspath(f)) for f in files]
        self._call("Cleanup", dbus_as(urls), mtime_threshold)

    def copy(self, from_files: List[str], to_files: List[str]) -> None:
        from_uris = [url_from_path(f) for f in from_files]
        to_uris = [url_from_path(f) for f in to_files]
        self._call("Copy", dbus_as(from_uris), dbus_as(to_uris))

    def move(self, from_files: List[str], to_files: List[str]) -> None:
        from_uris = [url_from_path(f) for f in from_files]
        to_uris = [url_from_path(f) for f in to_files]
        self._call("Move", dbus_as(from_uris), dbus_as(to_uris))


# EOF #
