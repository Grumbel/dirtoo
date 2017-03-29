#!/usr/bin/python3

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


import os
import sys
import argparse
import signal

from PyQt5.QtWidgets import QApplication

from dirtools.fileview.file_view_window import FileViewWindow


def parse_args(args):
    parser = argparse.ArgumentParser(description="Display files graphically")
    parser.add_argument("FILE", nargs='*')
    parser.add_argument("-t", "--timespace", action='store_true',
                        help="Space items appart given their mtime")
    parser.add_argument("-0", "--null", action='store_true',
                        help="Read \\0 separated lines")
    parser.add_argument("-r", "--recursive", action='store_true',
                        help="Be recursive")
    return parser.parse_args(args)


def expand_file(f, recursive):
    if os.path.isdir(f):
        if recursive:
            lst = [expand_file(os.path.join(f, x), recursive) for x in os.listdir(f)]
            return [item for sublist in lst for item in sublist]
        else:
            return [os.path.join(f, x) for x in os.listdir(f)]
    else:
        return [f]


def expand_directories(files, recursive):
    results = []
    for f in files:
        results += expand_file(f, recursive)
    return results


def get_file_list(args):
    if args.null:
        return filter(bool, sys.stdin.read().split('\0'))
    elif args.FILE == []:
        return sys.stdin.read().splitlines()
    else:
        return args.FILE


def main(argv):
    # Allow Ctrl-C killing of the Qt app, see:
    # http://stackoverflow.com/questions/4938723/
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    args = parse_args(argv[1:])
    files = get_file_list(args)

    app = QApplication([])
    window = FileViewWindow()
    window.file_view.timespace = args.timespace
    files = expand_directories(files, args.recursive)
    window.file_view.add_files(files)
    window.thumb_view.add_files(files)
    window.show()

    rc = app.exec_()

    # All Qt classes need to be destroyed before the app exists or we
    # get a segfault
    del window
    del app

    sys.exit(rc)


def main_entrypoint():
    main(sys.argv)


# EOF #
