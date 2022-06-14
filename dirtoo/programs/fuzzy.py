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


from typing import List, IO, Optional

import sys
import argparse

from dirtoo.fuzzy import fuzzy


def parse_args(args: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Seach for fuzzily file content")
    parser.add_argument("QUERY", nargs=1)
    parser.add_argument("FILE", nargs='*')
    parser.add_argument("-N", metavar="N", type=int, default=3,
                        help="Size of the N-grams")
    parser.add_argument("-n", "--line-number", action='store_true', default=False,
                        help="Prefix the output with line numbers")
    parser.add_argument("-t", "--threshold", metavar="INT", type=float, default=0.5,
                        help="Threshold for fuzzy matched")
    return parser.parse_args(args)


def process_stream(query: str, filename: Optional[str], fin: IO,
                   fuzzy_options,
                   args) -> None:
    for idx, line in enumerate(fin):
        line_no = idx + 1

        if fuzzy(query, line, n=fuzzy_options.n) > fuzzy_options.threshold:
            if filename is not None:
                sys.stdout.write("{}:{}: {}".format(filename, line_no, line))
            elif args.line_number:
                sys.stdout.write("{}: {}".format(line_no, line))
            else:
                sys.stdout.write(line)


class FuzzyOptions:

    def __init__(self, n, threshold):
        self.n = n
        self.threshold = threshold


def main(argv: List[str]) -> None:
    args = parse_args(argv[1:])

    query = args.QUERY[0]
    files = args.FILE
    fuzzy_options = FuzzyOptions(args.N, args.threshold)

    if len(query) < fuzzy_options.n:
        print("error: ngram size can't be larger than query string", file=sys.stderr)
        return

    if files == []:
        process_stream(query, None, sys.stdin, fuzzy_options, args)
    else:
        for filename in files:
            with open(filename) as fin:
                process_stream(query, filename, fin, fuzzy_options, args)


def main_entrypoint() -> None:
    main(sys.argv)


# EOF #
