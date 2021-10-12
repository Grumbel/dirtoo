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
import os
import shlex
import sys


from dirtools.archiveinfo import ArchiveInfo


def parse_args(argv):
    parser = argparse.ArgumentParser(description="Print Archive Information")
    parser.add_argument('FILE', metavar='FILE', type=str, nargs='+', help='Archive files to process')
    parser.add_argument('--file-count', action='store_true', default=False,
                        help="Print number of files inside the archive")
    return parser.parse_args(argv[1:])


def main(argv):
    args = parse_args(argv)
    for filename in args.FILE:
        _, ext = os.path.splitext(filename)
        try:
            archiveinfo = ArchiveInfo.from_file(filename)

            if args.file_count:
                print("{}\t{}".format(archiveinfo.file_count, filename))
            else:
                print("Archive: {}".format(shlex.quote(filename)))
                archiveinfo.print_summary()
                print()

        except libarchive.exception.ArchiveError as err:
            print("{}: error: couldn't process archive: {}".format(filename, err))
            continue


def main_entrypoint():
    main(sys.argv)


if __name__ == "__main__":
    main_entrypoint()


# EOF #
