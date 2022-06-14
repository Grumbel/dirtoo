# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2021 Ingo Ruhnke <grumbel@gmail.com>
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

import sys
import argparse
import random

from dirtoo.util import read_lines_from_files

logger = logging.getLogger(__name__)


def parse_args(args: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Shuffle lines")

    parser.add_argument("FILE", nargs='*')

    parser.add_argument("-0", "--null", action='store_true', default=False,
                        help="Read NUL separated lines")
    parser.add_argument("-s", "--seed", type=int, default=None,
                        help="Random seed")

    return parser.parse_args(args)


def main(argv):
    logging.basicConfig(level=logging.WARNING)

    args = parse_args(argv[1:])

    rnd = random.Random(args.seed)

    lines = read_lines_from_files(args.FILE, args.null)
    rnd.shuffle(lines)

    if args.null:
        for line in lines:
            sys.stdout.buffer.write(line)
            sys.stdout.buffer.write(b"\0")
    else:
        for line in lines:
            print(line)


def main_entrypoint():
    main(sys.argv)


# EOF #
