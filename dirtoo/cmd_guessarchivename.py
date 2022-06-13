# guessarchivename - Suggest new filename from archive file content
# Copyright (C) 2017 Ingo Ruhnke <grumbel@gmail.com>
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
import libarchive
import itertools
import os
import sys
from collections import defaultdict


def rename_safe(src: str, dst: str) -> None:
    if os.path.exists(dst):
        raise FileExistsError(dst)
    else:
        os.rename(src, dst)


def most_common(lst, threshold):
    el = max(set(lst), key=lst.count)
    if lst.count(el) < len(lst) * threshold:
        return None
    else:
        return el


def common_prefix(lst, threshold):
    m: defaultdict = defaultdict(list)
    for text in lst:
        for i, c in enumerate(text):
            m[i].append(c)

    result = [most_common(m[cs], threshold) for cs in sorted(m)]
    result = list(itertools.takewhile(lambda x: x is not None and x != "/", result))
    return "".join(result)


def strip_unimportant(text: str):
    return text.rstrip("-_0.")


def parse_args(argv):
    parser = argparse.ArgumentParser(description="Find common prefix from archive content")
    parser.add_argument('FILE', metavar='FILE', type=str, nargs='+', help='Archive file to search')
    parser.add_argument('-t', '--threshold', type=float, default=0.8,
                        help="Threshold for when a character is considered common (default: 0.8)")
    parser.add_argument('-E', '--ignore-ext', action='store_true',
                        help="Don't include file extension in the result")
    parser.add_argument('-M', '--min-length', type=int, default=5,
                        help="Minimum result length that is considered valid")
    parser.add_argument('-m', '--move', action='store_true',
                        help="Rename files to the suggested names")
    parser.add_argument('-i', '--interactive', action='store_true',
                        help="Ask before renaming each file")
    parser.add_argument('-b', '--basename', action='store_true',
                        help="Use basename only, not the full path for guessing")
    parser.add_argument('-s', '--strip', action='store_true',
                        help="Strip unimportant characters from the resulting string")
    parser.add_argument('-p', '--preserve', action='store_true',
                        help="Preserves the original filename by prepending it to the output")
    parser.add_argument('-v', '--verbose', action='store_true',
                        help="Be more verbose")
    return parser.parse_args(argv[1:])


def main(argv):
    args = parse_args(argv)

    for filename in args.FILE:
        basedir = os.path.dirname(filename)
        basename = os.path.basename(filename)
        original_filename, ext = os.path.splitext(basename)

        lst = []
        try:
            with libarchive.file_reader(filename) as archive:
                for entry in archive:
                    if entry.isfile:
                        lst.append(str(entry))
        except libarchive.exception.ArchiveError as err:
            print("{}: error: couldn't process archive: {}".format(filename, err))
            continue

        if args.basename:
            lst = [os.path.basename(p) for p in lst]

        common = common_prefix(lst, args.threshold)

        if len(common) < args.min_length:
            print("{}: error: could not find common prefix: '{}'".format(filename, common))
            continue

        if args.strip:
            common = strip_unimportant(common)

        if args.preserve:
            common = original_filename + "_" + common

        if not args.ignore_ext:
            common += ext

        if args.move:
            outfilename = os.path.join(basedir, common)
            if args.interactive:
                print("rename", filename, "->", common, "(y/n)?")
                answer = None
                while answer not in ["y", "n"]:
                    answer = input()
                if answer == "y":
                    print(filename, "->", common)
                    rename_safe(filename, outfilename)
                else:
                    print("ignoring", filename)
            else:
                print(filename, "->", common)
                rename_safe(filename, outfilename)
        elif args.verbose or len(args.FILE) > 1:
            print(filename, "->", common)
        else:
            print(common)


def main_entrypoint():
    main(sys.argv)


if __name__ == "__main__":
    main_entrypoint()


# EOF #
