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


import signal
import dbus
import argparse
import sys
import os

from PyQt5.QtCore import QCoreApplication
from dbus.mainloop.pyqt5 import DBusQtMainLoop

from dirtools.thumbnailer import Thumbnailer, ThumbnailerListener


def parse_args(args):
    parser = argparse.ArgumentParser(description="Make thumbnails for files")
    parser.add_argument("FILE", nargs='*')
    parser.add_argument('-f', '--flavor', metavar="FLAVOR", type=str, default="all",
                        help="Thumbnail size to generate (normal, large, all)")
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be more verbose")
    parser.add_argument('-r', '--recursive', action='store_true', default=False,
                        help="Recurse into directories")
    parser.add_argument('-F', '--list-flavors', action='store_true', default=False,
                        help="List supported flavors")
    parser.add_argument('-M', '--list-mime-types', action='store_true', default=False,
                        help="List supported mime-types")
    parser.add_argument('-U', '--list-uri-types', action='store_true', default=False,
                        help="List supported URI types")
    parser.add_argument('-S', '--list-schedulers', action='store_true', default=False,
                        help="List supported schedulers")
    return parser.parse_args(args)


class ThumbnailerProgressListener(ThumbnailerListener):

    def __init__(self, app, verbose):
        self.app = app
        self.verbose = verbose

    def started(self, handle):
        # print("[Started]", handle)
        pass

    def ready(self, handle, urls, flavor):
        if self.verbose:
            for url in urls:
                print(url, "->", Thumbnailer.thumbnail_from_url(url, flavor))

    def error(self, handle, failed_uris, error_code, message):
        for uri in failed_uris:
            print("[Error {}] {}: {}".format(error_code, uri, message), file=sys.stderr)

    def finished(self, handle):
        # print("[Finished]", handle)
        pass

    def idle(self):
        self.app.quit()


def request_thumbnails_recursive(thumber, directory, flavor):
    for root, dirs, files in os.walk(directory):
        thumber.queue([os.path.join(root, f) for f in files], flavor)
        for d in dirs:
            request_thumbnails_recursive(thumber, os.path.join(root, d), flavor)


def request_thumbnails(thumber, paths, flavor, recursive):
    if recursive:
        files = []
        dirs = []
        for p in paths:
            if os.path.isdir(p):
                dirs.append(p)
            else:
                files.append(p)
        thumber.queue(files, flavor)
        for d in dirs:
            request_thumbnails_recursive(thumber, d, flavor)
    else:
        thumber.queue(paths, flavor)


def main(argv):
    args = parse_args(argv[1:])

    signal.signal(signal.SIGINT, signal.SIG_DFL)
    app = QCoreApplication([])

    dbus_loop = DBusQtMainLoop(set_as_default=True)  # noqa: F841
    session_bus = dbus.SessionBus()

    thumber = Thumbnailer(session_bus,
                          ThumbnailerProgressListener(app,
                                                      verbose=args.verbose))

    rc = 0

    if args.list_flavors:
        for flavor in thumber.get_flavors():
            print(flavor)
    elif args.list_uri_types:
        uri_types, mime_types = thumber.get_supported()

        for uri_type in sorted(set(uri_types)):
            print(uri_type)
    elif args.list_mime_types:
        uri_types, mime_types = thumber.get_supported()

        for mime_type in sorted(set(mime_types)):
            print(mime_type)
    elif args.list_schedulers:
        for scheduler in thumber.get_schedulers():
            print(scheduler)
    elif args.FILE != []:
        if args.flavor == 'all':
            for flavor in thumber.get_flavors():
                request_thumbnails(thumber, args.FILE, flavor, args.recursive)
        else:
            request_thumbnails(thumber, args.FILE, args.flavor, args.recursive)

        rc = app.exec_()
    else:
        pass

    return rc


def main_entrypoint():
    exit(main(sys.argv))


# EOF #
