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


from typing import List

import argparse
import os
import sys
import uuid

from dirtools.filesystem import Filesystem


def parse_args(args: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Swap the names of files around")
    parser.add_argument("FILE1", nargs=1)
    parser.add_argument("FILE2", nargs=1)
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be more verbose")
    parser.add_argument('-n', '--dry-run', action='store_true', default=False,
                        help="Don't acutally execute")
    return parser.parse_args(args)


def main(argv: List[str]) -> int:
    args = parse_args(argv[1:])

    fs = Filesystem()
    fs.verbose = args.verbose
    fs.set_enabled(not args.dry_run)

    dir1 = os.path.dirname(args.FILE1[0])
    tmp1 = ".{}".format(uuid.uuid4())
    tmp1 = os.path.join(dir1, tmp1)

    fs.rename(args.FILE1[0], tmp1)
    fs.rename(args.FILE2[0], args.FILE1[0])
    fs.rename(tmp1, args.FILE2[0])

    return 0


def main_entrypoint():
    main(sys.argv)


# EOF #
