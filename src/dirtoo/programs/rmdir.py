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


from typing import Sequence

import argparse
import sys
import os


NAME = os.path.basename(sys.argv[0])


def remove_directory_recursive(directory: str, verbose: bool) -> None:
    for root, dirs, files in os.walk(directory, topdown=False):
        remove_directory(root, verbose)


def remove_directory(directory: str, verbose: bool) -> None:
    if verbose:
        print("{}: removing directory, '{}'".format(NAME, directory))
    try:
        os.rmdir(directory)
    except Exception as err:
        raise Exception("failed to remove '{}': {}".format(directory, err)) from err


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Remove empty directories")
    parser.add_argument("DIRECTORY", nargs="+", type=str)
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be verbose")
    parser.add_argument("-r", "--recursive", action='store_true', default=False,
                        help="Remove directories recursively")
    return parser.parse_args(argv[1:])


def main(argv: Sequence[str]) -> None:
    args = parse_args(argv)

    for directory in args.DIRECTORY:
        try:
            if args.recursive:
                remove_directory_recursive(directory, verbose=args.verbose)
            else:
                remove_directory(directory, verbose=args.verbose)
        except Exception as err:
            print("{}: {}".format(NAME, err), file=sys.stderr)


def main_entrypoint() -> None:
    main(sys.argv)


# EOF #
