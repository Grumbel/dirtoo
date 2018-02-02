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

from dirtools.mediainfo import MediaInfo


def parse_args():
    parser = argparse.ArgumentParser(description="Wrapper around MediaInfo")
    parser.add_argument('PATH', action='store', nargs='+',
                        help='Files to scan')
    return parser.parse_args()


def print_mediainfo(filename):
    mediainfo = MediaInfo(filename)

    hours, minutes, seconds = mediainfo.duration_tuple()

    print("{:02d}h:{:02d}m:{:02d}s  {:>6.2f}fps  {:>9}  {}".format(
        hours, minutes, seconds,
        mediainfo.framerate(),
        "{}x{}".format(mediainfo.width(), mediainfo.height()),
        filename))


def main():
    args = parse_args()
    for filename in args.PATH:
        print_mediainfo(filename)


# EOF #
