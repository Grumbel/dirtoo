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


def url_from_path(path):
    return "file://{}".format(urllib.parse.quote(os.path.abspath(path)))


class DBusThumbnailCache:

    def __init__(self, bus):
        self.cache = bus.get_object(
            'org.freedesktop.thumbnails.Cache1',
            '/org/freedesktop/thumbnails/Cache1')

    def delete(self, files):
        urls = [url_from_path(f) for f in files]
        self.cache.Delete(urls,
                          dbus_interface="org.freedesktop.thumbnails.Cache1")

    def cleanup(self, files, mtime_threshold=0):
        urls = ["file://" + urllib.parse.quote(os.path.abspath(f)) for f in files]
        self.cache.Cleanup(urls, mtime_threshold,
                           dbus_interface="org.freedesktop.thumbnails.Cache1")

    def copy(self, from_files, to_files):
        from_uris = [url_from_path(f) for f in from_files]
        to_uris = [url_from_path(f) for f in to_files]
        self.cache.Copy(from_uris, to_uris)

    def move(self, from_files, to_files):
        from_uris = [url_from_path(f) for f in from_files]
        to_uris = [url_from_path(f) for f in to_files]
        self.cache.Move(from_uris, to_uris)


# EOF #
