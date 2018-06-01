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
from dirtools.expr import Parser, Context


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
    parser = Parser()

    dt = mediainfo.duration_tuple()

    ctx = Context()

    ctx.set_variable('duration', mediainfo.duration())
    ctx.set_variable('hours', dt[0])
    ctx.set_variable('minutes', dt[1])
    ctx.set_variable('seconds', dt[2])
    ctx.set_variable('framerate', mediainfo.framerate())
    ctx.set_variable('width', mediainfo.width())
    ctx.set_variable('height', mediainfo.height())
    ctx.set_variable('filesize', bytefmt.humanize(mediainfo.filesize(), compact=True))
    ctx.set_variable('filename', mediainfo.filename())

    fmt = string.Formatter()
    for (literal_text, field_name, format_spec, _) in fmt.parse(fmt_str):
        if literal_text is not None:
            sys.stdout.write(literal_text)

        if field_name is not None:
            value = parser.eval(field_name, ctx)
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
