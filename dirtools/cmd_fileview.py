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

import logging
import os
import sys
import argparse

from dirtools.fileview.application import FileViewApplication
from dirtools.fileview.filelist_stream import FileListStream
from dirtools.fileview.location import Location
from dirtools.fileview.profiler import activate_profiler
from dirtools.util import expand_directories

logger = logging.getLogger(__name__)


def parse_args(args: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Display files graphically")
    parser.add_argument("FILE", nargs='*')
    parser.add_argument("-t", "--timespace", action='store_true',
                        help="Space items appart given their mtime")
    parser.add_argument("-0", "--null", action='store_true',
                        help="Read \\0 separated lines")
    parser.add_argument("-r", "--recursive", action='store_true', default=False,
                        help="Be recursive")
    parser.add_argument("-e", "--empty", action='store_true', default=False,
                        help="Start with a empty workbench instead of the current directory")
    parser.add_argument("-d", "--debug", action='store_true', default=False,
                        help="Print lots of debugging output")
    parser.add_argument("-p", "--profile", action='store_true', default=False,
                        help="Print profiling information")
    return parser.parse_args(args)


def main(argv: List[str]) -> None:
    args = parse_args(argv[1:])

    if args.debug:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.WARNING)

    if args.profile:
        activate_profiler(True)

    app = FileViewApplication()

    if args.FILE == []:
        app.show_location(Location.from_path(os.getcwd()))
    elif args.FILE == ["-"]:
        if args.null:
            app.show_filelist_stream(FileListStream(app.vfs, sys.stdin, "\0"))
        else:
            app.show_filelist_stream(FileListStream(app.vfs, sys.stdin, "\n"))
    elif len(args.FILE) == 1 and os.path.isdir(args.FILE[0]):
        app.show_location(Location.from_human(args.FILE[0]))
    elif args.recursive:
        files = expand_directories(args.FILE, args.recursive)
        app.show_files([Location.from_path(f) for f in files])
    else:
        app.show_files([Location.from_human(f) for f in args.FILE])

    sys.exit(app.run())


def main_entrypoint() -> None:
    main(sys.argv)


# EOF #
