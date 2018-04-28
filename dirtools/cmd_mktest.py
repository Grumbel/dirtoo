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
import sys
import os


def parse_args(args: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Create create a file hierachy for testing")
    parser.add_argument("DIRECTORY", nargs=1, help="Create files in DIRECTORY")
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be verbose")
    parser.add_argument('-D', '--depth', metavar="N", type=int, default=3,
                        help="Create a hierachy that is N layers deep")
    parser.add_argument('-f', '--files', metavar="N", type=int, default=3,
                        help="Create N files per directory")
    parser.add_argument('-d', '--directories', metavar="N", type=int, default=3,
                        help="Create N directories per directory")
    return parser.parse_args(args)


def make_test(path: str, depth: int, args: argparse.Namespace) -> None:
    for idx in range(args.files):
        outfile = os.path.join(path, "depth{}-file{:04d}".format(args.depth - depth, idx))

        if args.verbose:
            print("writing {}".format(outfile))

        with open(outfile, "w") as fout:
            fout.write(outfile + "\n")

    for idx in range(args.directories):
        outdir = os.path.join(path, "depth{}-dir{:04d}".format(args.depth - depth, idx))

        if args.verbose:
            print("creating {}".format(outdir))

        os.mkdir(outdir)
        if depth > 0:
            make_test(outdir, depth - 1, args)


def main(argv):
    args = parse_args(argv[1:])

    directory = args.DIRECTORY[0]

    if not os.path.isdir(directory):
        os.mkdir(directory)

    make_test(directory, args.depth, args)

    return 0


def main_entrypoint():
    sys.exit(main(sys.argv))


# EOF #
