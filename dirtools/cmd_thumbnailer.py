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


from typing import List

import signal
import argparse
import sys
import os

from PyQt5.QtCore import QCoreApplication
from PyQt5.QtDBus import QDBusConnection

from dirtools.dbus_thumbnailer import DBusThumbnailer, DBusThumbnailerListener
from dirtools.dbus_thumbnail_cache import DBusThumbnailCache


def parse_args(args: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Make thumbnails for files")
    parser.add_argument("FILE", nargs='*')
    parser.add_argument('-f', '--flavor', metavar="FLAVOR", type=str, default="all",
                        help="Thumbnail size to generate (normal, large, all)")
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be more verbose")
    parser.add_argument('-r', '--recursive', action='store_true', default=False,
                        help="Recurse into directories")
    parser.add_argument('-d', '--delete', action='store_true', default=False,
                        help="Delete the thumbnails for the given files")
    parser.add_argument('-c', '--cleanup', action='store_true', default=False,
                        help="Cleanup the thumbnails under the given directories")
    parser.add_argument('-F', '--list-flavors', action='store_true', default=False,
                        help="List supported flavors")
    parser.add_argument('-M', '--list-mime-types', action='store_true', default=False,
                        help="List supported mime-types")
    parser.add_argument('-U', '--list-uri-types', action='store_true', default=False,
                        help="List supported URI types")
    parser.add_argument('-S', '--list-schedulers', action='store_true', default=False,
                        help="List supported schedulers")
    return parser.parse_args(args)


class ThumbnailerProgressListener(DBusThumbnailerListener):

    def __init__(self, app, verbose: bool) -> None:
        super().__init__()

        self.app = app
        self.verbose = verbose

    def started(self, handle) -> None:
        # print("[Started]", handle)
        pass

    def ready(self, handle, urls, flavor) -> None:
        if self.verbose:
            for url in urls:
                print(url, "->", DBusThumbnailer.thumbnail_from_url(url, flavor))

    def error(self, handle, failed_uris, error_code, message) -> None:
        for uri in failed_uris:
            print("[Error {}] {}: {}".format(error_code, uri, message), file=sys.stderr)

    def finished(self, handle) -> None:
        pass

    def idle(self) -> None:
        self.app.quit()


def request_thumbnails_recursive(thumbnailer, directory, flavor):
    for root, dirs, files in os.walk(directory):
        thumbnailer.queue([os.path.join(root, f) for f in files], flavor)
        for d in dirs:
            request_thumbnails_recursive(thumbnailer, os.path.join(root, d), flavor)


def request_thumbnails(thumbnailer, paths, flavor, recursive):
    if recursive:
        files = []
        dirs = []
        for p in paths:
            if os.path.isdir(p):
                dirs.append(p)
            else:
                files.append(p)
        thumbnailer.queue(files, flavor)
        for d in dirs:
            request_thumbnails_recursive(thumbnailer, d, flavor)
    else:
        thumbnailer.queue(paths, flavor)


def main(argv):
    args = parse_args(argv[1:])

    signal.signal(signal.SIGINT, signal.SIG_DFL)
    app = QCoreApplication([])

    session_bus = QDBusConnection.sessionBus()
    thumbnailer = DBusThumbnailer(session_bus,
                                  ThumbnailerProgressListener(
                                      app, verbose=args.verbose))
    thumbnail_cache = DBusThumbnailCache(session_bus)
    rc = 0

    if args.list_flavors:
        for flavor in thumbnailer.get_flavors():
            print(flavor)
    elif args.list_uri_types:
        uri_types, mime_types = thumbnailer.get_supported()

        for uri_type in sorted(set(uri_types)):
            print(uri_type)
    elif args.list_mime_types:
        uri_types, mime_types = thumbnailer.get_supported()

        for mime_type in sorted(set(mime_types)):
            print(mime_type)
    elif args.list_schedulers:
        for scheduler in thumbnailer.get_schedulers():
            print(scheduler)
    elif args.delete:
        thumbnail_cache.delete(args.FILE)
        app.quit()
    elif args.cleanup:
        thumbnail_cache.cleanup(args.FILE)
        app.quit()
    elif args.FILE != []:
        if args.flavor == 'all':
            for flavor in thumbnailer.get_flavors():
                request_thumbnails(thumbnailer, args.FILE, flavor, args.recursive)
        else:
            request_thumbnails(thumbnailer, args.FILE, args.flavor, args.recursive)

        rc = app.exec()
    else:
        pass

    return rc


def main_entrypoint():
    exit(main(sys.argv))


# EOF #
