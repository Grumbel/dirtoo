#!/usr/bin/env python3

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
from ctypes import c_void_p, c_size_t, c_char_p
import MediaInfoDLL3

# Monkey patching a bug in MediaInfoDLL3
MediaInfoDLL3.MediaInfo.MediaInfoA_Get.argtypes = [
    c_void_p, c_size_t, c_size_t, c_char_p, c_size_t, c_size_t
]


def parse_args():
    parser = argparse.ArgumentParser(description="Wrapper around MediaInfo")
    parser.add_argument('PATH', action='store', nargs='+',
                        help='Files to scan')
    return parser.parse_args()


def print_mediainfo(filename):
    milib = MediaInfoDLL3.MediaInfo()
    ret = milib.Open(filename)
    if ret != 1:
        print("Error")
        return

    # general_count = milib.Count_Get(MediaInfoDLL3.Stream.General)
    # video_count = milib.Count_Get(MediaInfoDLL3.Stream.Video)
    # audio_count = milib.Count_Get(MediaInfoDLL3.Stream.Audio)

    duration = milib.Get(MediaInfoDLL3.Stream.General, 0, "Duration")
    # bitrate = milib.Get(MediaInfoDLL3.Stream.General, 0, "OverallBitRate")
    # filesize = milib.Get(MediaInfoDLL3.Stream.General, 0, "FileSize")
    framerate = milib.Get(MediaInfoDLL3.Stream.General, 0, "FrameRate")

    width = milib.Get(MediaInfoDLL3.Stream.Video, 0, "Width")
    height = milib.Get(MediaInfoDLL3.Stream.Video, 0, "Height")

    milib.Close()

    framerate = float(framerate) if framerate else 0
    duration = int(duration)
    width = int(width)
    height = int(height)

    hours = duration // 1000 // 60 // 60
    duration -= 1000 * 60 * 60 * hours
    minutes = duration // 1000 // 60
    duration -= 1000 * 60 * minutes
    seconds = duration // 1000

    print("{:02d}h:{:02d}m:{:02d}s  {:>6.2f}fps  {:>9}  {}".format(
        hours, minutes, seconds,
        framerate,
        "{}x{}".format(width, height),
        filename))


def main():
    args = parse_args()
    for filename in args.PATH:
        print_mediainfo(filename)


# EOF #
