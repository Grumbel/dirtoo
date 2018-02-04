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

# https://dbus.freedesktop.org/doc/dbus-python/doc/tutorial.html
# https://wiki.gnome.org/action/show/DraftSpecs/ThumbnailerSpec
# qdbus org.freedesktop.thumbnails.Thumbnailer1 /org/freedesktop/thumbnails/Thumbnailer1


from typing import Dict, Tuple, List

import os
import hashlib
import urllib.parse
import dbus
import mimetypes
import xdg.BaseDirectory
from enum import Enum


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


class DBusThumbnailer:

    def __init__(self, bus, listener=None):
        self.bus = bus
        self.requests: Dict[str, Tuple[str, str, str]] = {}
        self.thumbnailer_obj = bus.get_object(
            'org.freedesktop.thumbnails.Thumbnailer1',
            '/org/freedesktop/thumbnails/Thumbnailer1',
            follow_name_owner_changes=True)

        self.thumbnailer_if = dbus.Interface(
            self.thumbnailer_obj,
            dbus_interface="org.freedesktop.thumbnails.Thumbnailer1")

        self.listener = listener

        self.thumbnailer_if.connect_to_signal("Ready", self._receive_ready)
        self.thumbnailer_if.connect_to_signal("Started", self._receive_started)
        self.thumbnailer_if.connect_to_signal("Finished", self._receive_finished)
        self.thumbnailer_if.connect_to_signal("Error", self._receive_error)

    def _add_request(self, handle, data):
        self.requests[handle] = data

    def _remove_request(self, handle):
        if handle in self.requests:
            del self.requests[handle]

        if not self.requests:
            self.listener.idle()

    def _receive_started(self, handle):
        self.listener.started(int(handle))

    def _receive_ready(self, handle, uris):
        data = self.requests[handle]
        self.listener.ready(int(handle), uris, data[2])

    def _receive_finished(self, handle):
        self.listener.finished(int(handle))
        self._remove_request(int(handle))

    def _receive_error(self, handle, failed_uris, error_code, message):
        self.listener.error(handle, failed_uris, error_code, message)

    def queue(self, files: List[str], flavor: str):
        if files == []:
            return

        urls = ["file://" + urllib.parse.quote(os.path.abspath(f)) for f in files]
        mime_types = [
            mimetypes.guess_type(url)[0] or "application/octet-stream"
            for url in urls
        ]
        handle = self.thumbnailer_if.Queue(
            urls,  # uris: as
            mime_types,  # mime_types: as
            flavor,  # flavor: s
            "default",  # scheduler: s
            dbus.UInt32(0),  # handle_to_dequeue: u
            # <arg type="u" name="handle" direction="out" />
        )
        self._add_request(int(handle), (urls, mime_types, flavor))
        return int(handle)

    def dequeue(self, handle):
        self.thumbnailer_if.Dequeue(handle)
        del self.requests[handle]

    def get_supported(self):
        uri_schemes, mime_types = self.thumbnailer_if.GetSupported()
        return (uri_schemes, mime_types)

    def get_schedulers(self):
        schedulers = self.thumbnailer_if.GetSchedulers()
        return schedulers

    def get_flavors(self):
        flavors = self.thumbnailer_if.GetFlavors()
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
