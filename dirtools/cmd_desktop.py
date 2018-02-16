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


import argparse
import os
import sys

from xdg.DesktopEntry import DesktopEntry
from xdg.BaseDirectory import xdg_data_dirs

from dirtools.xdg_desktop import get_desktop_file


# https://standards.freedesktop.org/desktop-entry-spec/latest/


def parse_args(args):
    parser = argparse.ArgumentParser(description="Query the systems .desktop files")
    parser.add_argument("DESKTOP", nargs='?')
    return parser.parse_args(args)


def main(argv):
    args = parse_args(argv[1:])

    if args.DESKTOP is None:
        for directory in xdg_data_dirs:
            print(os.path.join(directory, "applications"))
    else:
        filename = get_desktop_file(args.DESKTOP)
        print(filename)
        desktop = DesktopEntry(filename)
        print("Name: {}".format(desktop.getName()))
        print("Exec: {}".format(desktop.getExec()))
        print("TryExec: {}".format(desktop.getTryExec()))
        print("Mime-Types: {}".format(desktop.getMimeTypes()))


def main_entrypoint():
    exit(main(sys.argv))


# EOF #
