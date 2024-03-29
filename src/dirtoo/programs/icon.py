# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2015 Ingo Ruhnke <grumbel@gmail.com>
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


from typing import Sequence

import argparse
import logging
import os
import sys

from PyQt6.QtCore import QMimeDatabase
from PyQt6.QtGui import QIcon
from PyQt6.QtWidgets import QApplication

logger = logging.getLogger(__name__)


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Find icons")
    parser.add_argument("ICON", nargs='*')
    parser.add_argument("-l", "--list", action='store_true', default=False,
                        help="List available icons")
    parser.add_argument("-m", "--mime-type", action='store_true', default=False,
                        help="Lookup icons by mime-type")
    parser.add_argument("--list-mime", action='store_true', default=False,
                        help="List all mime types")
    return parser.parse_args(argv[1:])


def main(argv: Sequence[str]) -> None:
    args = parse_args(argv)

    app = QApplication([])  # noqa: F841

    if args.list:
        for idx, path in enumerate(QIcon.themeSearchPaths()):
            for root, dirs, files in os.walk(path):
                for name in files:
                    if name.endswith(".png") or name.endswith(".svg"):
                        print(os.path.join(root, name))
    elif args.list_mime:
        mimedb = QMimeDatabase()
        for mt in mimedb.allMimeTypes():
            print(f"{mt.name():50}  {mt.iconName()}")
    else:
        if args.ICON == []:
            print("{:>17} : {}".format("Theme", QIcon.themeName()))
            for idx, path in enumerate(QIcon.themeSearchPaths()):
                if idx == 0:
                    print("{:>17} : {!r}".format("ThemeSearchPath", path))
                else:
                    print("{:>17}   {!r}".format("", path))
        else:
            if args.mime_type:
                mimedb = QMimeDatabase()
                for mimetype in args.ICON:
                    mt = mimedb.mimeTypeForName(mimetype)
                    if mt.isValid():
                        iconname = mt.iconName()
                        print("{}: {}  {}".format(mimetype, iconname,
                                                  "OK"
                                                  if QIcon.hasThemeIcon(iconname)
                                                  else "FAILED"))
                    else:
                        print("{}: invalid mime-type".format(mimetype))
            else:
                for iconname in args.ICON:
                    print("{}: {}".format(iconname, "OK" if QIcon.hasThemeIcon(iconname) else "FAILED"))


def main_entrypoint() -> None:
    main(sys.argv)


# EOF #
