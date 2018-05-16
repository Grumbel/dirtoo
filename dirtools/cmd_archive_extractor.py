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

from dirtools.extractor import make_extractor


def parse_args(args):
    parser = argparse.ArgumentParser(description="Extract archive files")
    parser.add_argument("ARCHIVE", nargs=1,
                        help="Archive to extract")
    parser.add_argument("-o", "--outdir", type=str, required=True,
                        help="Output directory for extraction")
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be verbose")
    return parser.parse_args(args)


def main(argv):
    args = parse_args(argv[1:])

    extractor = make_extractor(args.ARCHIVE[0], args.outdir)

    if args.verbose:
        extractor.sig_entry_extracted.connect(lambda x, y: print(y))

    extractor.extract()


def main_entrypoint():
    main(sys.argv)


# EOF #
