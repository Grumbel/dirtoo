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
import sys

logger = logging.getLogger(__name__)


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Remove trailing newline")
    parser.add_argument("FILE", nargs='*')
    return parser.parse_args(argv[1:])


def main(argv: Sequence[str]) -> None:
    args = parse_args(argv)

    content = ""
    if not args.FILE:
        content += sys.stdin.read()
    else:
        for filename in args.FILE:
            if filename == "-":
                content += sys.stdin.read()
            else:
                with open(filename) as fin:
                    content += fin.read()

    content = content.rstrip('\n')

    sys.stdout.write(content)


def main_entrypoint() -> None:
    main(sys.argv)


# EOF #
