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


from typing import List

import argparse
import fnmatch
import logging
import sys

logger = logging.getLogger(__name__)


def parse_args(argv: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Print lines matching glob pattern")
    parser.add_argument("PATTERN", nargs=1, help="Glob pattern")
    parser.add_argument("FILE", nargs="*", help="Files to search")

    parser.add_argument("-i", "--ignore-case", action='store_true',
                        help="Ignore case", default=False)
    parser.add_argument("-v", "--invert-match", action='store_true',
                        help="Invert the sense of matching", default=False)
    parser.add_argument("-H", "--with-filename", action='store_true',
                        help="Print the file name for each match", default=False)
    parser.add_argument("--no-filename", action='store_true',
                        help="Suppress  the  prefixing  of file names on output", default=False)  # -h

    return parser.parse_args()


def main(argv: List[str]) -> None:
    args = parse_args(argv)

    pattern = args.PATTERN[0]

    for line_no, filename in enumerate(args.FILE):
        with open(filename, "r") as fin:
            for line in fin:
                outline = ""

                print_filename = ((len(args.FILE) > 1 and not args.no_filename) or
                                  args.with_filename)

                if print_filename:
                    outline += "{}:{}: ".format(filename, line_no + 1)

                outline += "{}".format(line)

                if fnmatch.fnmatch(line, pattern):
                    if not args.invert_match:
                        sys.stdout.write(outline)
                else:
                    if args.invert_match:
                        sys.stdout.write(outline)


def main_entrypoint() -> None:
    main(sys.argv)


# EOF #
