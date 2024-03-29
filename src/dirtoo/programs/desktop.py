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


from typing import Sequence, Optional

import argparse
import os
import sys

from xdg.DesktopEntry import DesktopEntry
from xdg.BaseDirectory import xdg_data_dirs

from dirtoo.xdg_desktop import get_desktop_file


# https://standards.freedesktop.org/desktop-entry-spec/latest/


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Query the systems .desktop files")

    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("DESKTOP", nargs='?')
    group.add_argument('-l', '--list-dirs', action='store_true', default=False,
                       help="List all directories scanned for .desktop files")
    group.add_argument('-L', '--list-files', action='store_true', default=False,
                       help="List all .desktop files")

    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be verbose")
    return parser.parse_args(argv[1:])


def main(argv: Sequence[str]) -> int:
    args = parse_args(argv)

    if args.list_dirs:
        for directory in xdg_data_dirs:
            print(os.path.join(directory, "applications"))
    elif args.list_files:
        for directory in xdg_data_dirs:
            path = os.path.join(directory, "applications")
            try:
                for entry in os.listdir(path):
                    if entry.endswith(".desktop"):
                        if args.verbose:
                            desktop_filename = os.path.join(path, entry)
                            desktop = DesktopEntry(desktop_filename)
                            print("{:70}  {:40}  {:40}".format(desktop_filename, desktop.getName(), desktop.getExec()))
                        else:
                            print(os.path.join(path, entry))
            except FileNotFoundError:
                pass
    else:
        filename: Optional[str] = get_desktop_file(args.DESKTOP)
        if not filename:
            pass
        else:
            print(filename)
            desktop = DesktopEntry(filename)
            print("Name: {}".format(desktop.getName()))
            print("Exec: {}".format(desktop.getExec()))
            print("TryExec: {}".format(desktop.getTryExec()))
            print("Mime-Types: {}".format(desktop.getMimeTypes()))

    return 0


def main_entrypoint() -> None:
    sys.exit(main(sys.argv))


# EOF #
