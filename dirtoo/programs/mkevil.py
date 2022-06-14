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


import argparse
import sys
import os
import shlex


def parse_args(args):
    parser = argparse.ArgumentParser(description="Create files with unusual filenames for testing")
    parser.add_argument("DIRECTORY", nargs=1, help="Create files in DIRECTORY")
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be verbose")
    return parser.parse_args(args)


def main(argv):
    args = parse_args(argv[1:])

    files = [
        b'HelloWorld.txt',
        b'DoubleExtension.gif.jpg',
        b'NewLine:\n:End',
        b'InvalidUTF8:\xff:End',
        os.fsencode('UpsideDown:ʇsǝ⊥uʍo◖ǝpısd∩:End'),
        b'MaximumLength:' + b'X' * 237 + b':End'
    ]

    for filename_bytes in files:
        filename = os.fsdecode(filename_bytes)

        outfile = os.path.join(args.DIRECTORY[0], filename)

        if args.verbose:
            print("Creating {}".format(shlex.quote(filename).encode("utf-8", errors="ignore")))

        with open(outfile, "wb") as fout:
            fout.write(os.fsencode(shlex.quote(filename)) + b'\n')

    return 0


def main_entrypoint():
    sys.exit(main(sys.argv))


# EOF #
