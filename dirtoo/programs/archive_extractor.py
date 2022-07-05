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
import logging
import sys

from dirtoo.archive.extractor_factory import make_extractor


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Extract archive files")
    parser.add_argument("ARCHIVE", nargs=1,
                        help="Archive to extract")
    parser.add_argument("-o", "--outdir", type=str, required=True,
                        help="Output directory for extraction")
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be verbose")
    parser.add_argument('--debug', action='store_true', default=False,
                        help="Be even more verbose")
    return parser.parse_args(argv[1:])


def main(argv: Sequence[str]) -> None:
    args = parse_args(argv)

    if args.debug:
        logging.basicConfig(level=logging.DEBUG)

    extractor = make_extractor(args.ARCHIVE[0], args.outdir)

    if args.verbose:
        extractor.sig_entry_extracted.connect(lambda x, y: print(y))

    extractor.extract()


def main_entrypoint() -> None:
    main(sys.argv)


# EOF #
