#!/usr/bin/env python3

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
import sys

from dirtools.expr import Parser, Context


def parse_args(args):
    parser = argparse.ArgumentParser(description="Evaluate expressions")
    parser.add_argument("EXPRESSION", nargs='*')
    return parser.parse_args(args)


def main(argv):
    args = parse_args(argv[1:])

    parser = Parser()
    ctx = Context()
    for expression in args.EXPRESSION:
        print(expression)
        result = parser.eval(expression, ctx)
        print("Result: {}".format(result))


def main_entrypoint():
    main(sys.argv)


# EOF #
