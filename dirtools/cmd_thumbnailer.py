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
from PyQt5.QtCore import QCoreApplication
from dbus.mainloop.qt import DBusQtMainLoop
import dbus
import argparse
import sys
import os

from dirtools.thumbnail import make_thumbnail_filename


def parse_args(args):
    parser = argparse.ArgumentParser(description="Make thumbnails for files")
    parser.add_argument("FILE", nargs='+')
    parser.add_argument('-f', '--flavour', metavar="FLAVOUR", type=str, default="normal",
                        help="Generate flavour (normal, large)")
    return parser.parse_args(args)


def make_thumbnail(thumbnailer, filename, flavour):
    print("Generating thumbnail for", filename)

    handle = thumbnailer.Queue(
        ["file://" + os.path.abspath(filename)], # <arg type="as" name="uris" direction="in" />
        ["image/jpeg"], # <arg type="as" name="mime_types" direction="in" />
        flavour, # <arg type="s" name="flavor" direction="in" />
        "default", # <arg type="s" name="scheduler" direction="in" />
        dbus.UInt32(0), # <arg type="u" name="handle_to_dequeue" direction="in" />
        # <arg type="u" name="handle" direction="out" />
    )
    print(handle)

    #thumbnailer.connect_to_signal("org.freedesktop.thumbnails.Thumbnailer1.Finished",
    #                              handler,
    #                              sender_keyword='sender')

def signal_handler(*args, **kwargs):
    print("  ", kwargs)
    print("  ", args)
    print()


def main(argv):
    args = parse_args(argv[1:])

    dbus_loop = DBusQtMainLoop(set_as_default=True)
    session_bus = dbus.SessionBus()

    if False:
        session_bus.add_signal_receiver(
            signal_handler, # handler
            None, # signal name
            None, # dbus interface
            None, # bus name
            None, # sender object path
            sender_keyword="sender",
            destination_keyword="destination",
            interface_keyword="interface",
            member_keyword="member",
        )

    session_bus.add_signal_receiver(
        signal_handler,
        None,
        'org.freedesktop.thumbnails.Thumbnailer1',
        None,
        None,
        sender_keyword="sender",
        destination_keyword="destination",
        interface_keyword="interface",
        member_keyword="member",
    )

    thumbnailer = session_bus.get_object('org.freedesktop.thumbnails.Thumbnailer1',
                                        '/org/freedesktop/thumbnails/Thumbnailer1')
    print(dir(thumbnailer))

    print("Flavours:", thumbnailer.GetFlavors())
    print("Schedulers:", thumbnailer.GetSchedulers())
    # print("Supported:", thumbnailer.GetSupported())

    for filename in args.FILE:
        make_thumbnail(thumbnailer, filename, flavour=args.flavour)
        print(">>>", make_thumbnail_filename(filename, args.flavour))

    signal.signal(signal.SIGINT, signal.SIG_DFL)
    app = QCoreApplication([])
    rc = app.exec_()
    return rc


def main_entrypoint():
    exit(main(sys.argv))


# EOF #
