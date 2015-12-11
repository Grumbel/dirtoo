#!/usr/bin/env python3

# dirtool.py - diff tool for directories
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


import os
import argparse
import sys


def check_utf8(text, verbose):
    try:
        text.encode("utf-8")
        valid_utf8 = True
    except UnicodeEncodeError as err:
        valid_utf8 = False

    if valid_utf8:
        if verbose:
            sys.stdout.buffer.write(b"GOOD ")
            sys.stdout.buffer.write(os.fsencode(text))
            sys.stdout.buffer.write(b"\n")
    else:
        sys.stdout.buffer.write(b"FAIL ")
        sys.stdout.buffer.write(os.fsencode(text))
        sys.stdout.buffer.write(b"\n")


def check_utf8_path(path, recursive, verbose):
    if recursive and os.path.isdir(path):
        for root, dirs, files in os.walk(path):
            for f in files:
                check_utf8(os.path.join(root, f), verbose)
    else:
        check_utf8(path, verbose)


def parse_args():
    parser = argparse.ArgumentParser(description="Check filenames for invalid UTF-8")
    parser.add_argument('PATH', action='store', nargs='+',
                        help='File or directories to check')
    parser.add_argument('-r', '--recursive', action='store_true', default=False,
                        help="Recursivly check directories")
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be verbose")
    # option for absolute path
    return parser.parse_args()


def main():
    args = parse_args()

    for p in args.PATH:
        check_utf8_path(p, args.recursive, args.verbose)

if __name__ == "__main__":
    main()


# EOF #
