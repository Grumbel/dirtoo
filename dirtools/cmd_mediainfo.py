# dirtool.py - diff tool for directories
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
import bytefmt
import string
import sys

from dirtools.mediainfo import MediaInfo


def parse_args():
    parser = argparse.ArgumentParser(description="Wrapper around MediaInfo")
    parser.add_argument('PATH', action='store', nargs='+',
                        help='Files to scan')
    parser.add_argument("-p", "--println", metavar="FMT",
                        help="Print FMT (with newline)")
    parser.add_argument("-P", "--print", metavar="FMT",
                        help="Print FMT (without newline)")
    return parser.parse_args()


def format_output(mediainfo: MediaInfo, fmt_str: str):
    global_vars = {}

    dt = mediainfo.duration_tuple()

    local_vars = {
        'duration': mediainfo.duration(),
        'hours': dt[0],
        'minutes': dt[1],
        'seconds': dt[2],
        'framerate': mediainfo.framerate(),
        'width': mediainfo.width(),
        'height': mediainfo.height(),
        'filesize': bytefmt.humanize(mediainfo.filesize(), compact=True),
        'filename': mediainfo.filename()
    }

    fmt = string.Formatter()
    for (literal_text, field_name, format_spec, _) in fmt.parse(fmt_str):
        if literal_text is not None:
            sys.stdout.write(literal_text)

        if field_name is not None:
            # FIXME: use expr parser here, not eval
            value = eval(field_name, global_vars, local_vars)
            sys.stdout.write(format(value, format_spec))


def main():
    args = parse_args()

    fmt = ("{hours:02d}h:{minutes:02d}m:{seconds:02d}s  {filesize:>9}  "
           "{framerate:>6.2f}fps  {width}x{height}  {filename}\n")

    if args.print:
        fmt = args.print
    elif args.println:
        fmt = args.println + "\n"

    for filename in args.PATH:
        mediainfo = MediaInfo(filename)
        format_output(mediainfo, fmt)


# EOF #
