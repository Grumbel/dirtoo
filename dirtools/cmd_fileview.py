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


import sys
import argparse
import signal

from PyQt5.QtWidgets import QApplication

from dirtools.fileview.controller import Controller
from dirtools.util import expand_directories


def parse_args(args):
    parser = argparse.ArgumentParser(description="Display files graphically")
    parser.add_argument("FILE", nargs='*')
    parser.add_argument("-t", "--timespace", action='store_true',
                        help="Space items appart given their mtime")
    parser.add_argument("-0", "--null", action='store_true',
                        help="Read \\0 separated lines")
    parser.add_argument("-r", "--recursive", action='store_true',
                        help="Be recursive")
    parser.add_argument("-e", "--empty", action='store_true', default=False,
                        help="Start with a empty workbench instead of the current directory")
    return parser.parse_args(args)


def get_file_list(args):
    if args.null:
        return filter(bool, sys.stdin.read().split('\0'))
    elif args.FILE == ["-"]:
        return sys.stdin.read().splitlines()
    elif args.FILE == []:
        if args.empty:
            return []
        else:
            return ["."]
    else:
        return args.FILE


def main(argv):
    # Allow Ctrl-C killing of the Qt app, see:
    # http://stackoverflow.com/questions/4938723/
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    args = parse_args(argv[1:])
    files = get_file_list(args)

    app = QApplication([])

    controller = Controller()

    files = expand_directories(files, args.recursive)
    controller.set_files(files)

    controller.window.show()

    sys.exit(app.exec_())


def main_entrypoint():
    main(sys.argv)


# EOF #
