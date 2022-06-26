#!/usr/bin/env python3

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
import sys

from dirtoo.expr import Parser, Context


def parse_args(argv: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Evaluate expressions")
    parser.add_argument("EXPRESSION", nargs='+')
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be verbose")
    return parser.parse_args(argv[1:])


def main(argv: List[str]) -> None:
    args = parse_args(argv)

    parser = Parser()
    ctx = Context()
    for expression in args.EXPRESSION:
        result = parser.eval(expression, ctx)
        if args.verbose:
            print("{} => {}".format(expression, result))
        else:
            print(result)


def main_entrypoint() -> None:
    main(sys.argv)


# EOF #
