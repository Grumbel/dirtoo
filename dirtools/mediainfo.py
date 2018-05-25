# dirtool.py - diff tool for directories
# Copyright (C) 2017,2018 Ingo Ruhnke <grumbel@gmail.com>
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


from typing import Tuple
from ctypes import c_void_p, c_size_t, c_char_p

import MediaInfoDLL3


# monkey patching a bug in MediaInfoDLL3
MediaInfoDLL3.MediaInfo.MediaInfoA_Get.argtypes = [
    c_void_p, c_size_t, c_size_t, c_char_p, c_size_t, c_size_t
]


def _to_float(text: str, default: float = 0.0) -> float:
    if text == "":
        return default
    else:
        return float(text)


def _to_int(text: str, default: int = 0) -> int:
    if text == "":
        return default
    else:
        try:
            return int(text)
        except ValueError:
            # mediainfo will sometimes return strings like "234234.0000"
            try:
                return int(float(text))
            except ValueError:
                return default
        else:
            return default


def split_duration(duration: int) -> Tuple[int, int, int]:
    hours = duration // 1000 // 60 // 60
    duration -= 1000 * 60 * 60 * hours
    minutes = duration // 1000 // 60
    duration -= 1000 * 60 * minutes
    seconds = duration // 1000

    return (hours, minutes, seconds)


class MediaInfo:

    def __init__(self, filename: str) -> None:
        self._filename = filename

        minfo = MediaInfoDLL3.MediaInfo()

        ret = minfo.Open(filename)
        if ret != 1:
            minfo.Close()
            # mediainfo isn't returning a reason for why the Open() call
            # has failed, so we try to open the file ourselves to produce
            # a more useful error message
            with open(filename):
                pass
            raise Exception("Error {}: cannot access {}".format(ret, filename))
        else:
            self._general_count = minfo.Count_Get(MediaInfoDLL3.Stream.General)
            self._video_count = minfo.Count_Get(MediaInfoDLL3.Stream.Video)
            self._audio_count = minfo.Count_Get(MediaInfoDLL3.Stream.Audio)
            self._image_count = minfo.Count_Get(MediaInfoDLL3.Stream.Image)

            # print(repr(minfo.Get(MediaInfoDLL3.Stream.Video, 0, "FrameRate_Mode/String"))) # CFR VFR
            # print(repr(minfo.Get(MediaInfoDLL3.Stream.Video, 0, "FrameRate_Maximum")))
            # print(repr(minfo.Get(MediaInfoDLL3.Stream.Video, 0, "FrameRate_Minimum/String")))

            self._duration = _to_int(minfo.Get(MediaInfoDLL3.Stream.General, 0, "Duration"))
            self._framerate = _to_float(minfo.Get(MediaInfoDLL3.Stream.General, 0, "FrameRate"))
            self._filesize = _to_int(minfo.Get(MediaInfoDLL3.Stream.General, 0, "FileSize"))
            # self._bitrate = minfo.Get(MediaInfoDLL3.Stream.General, 0, "OverallBitRate")

            if self._video_count > 0:
                self._width = _to_int(minfo.Get(MediaInfoDLL3.Stream.Video, 0, "Width"))
                self._height = _to_int(minfo.Get(MediaInfoDLL3.Stream.Video, 0, "Height"))
            elif self._image_count > 0:
                self._width = _to_int(minfo.Get(MediaInfoDLL3.Stream.Image, 0, "Width"))
                self._height = _to_int(minfo.Get(MediaInfoDLL3.Stream.Image, 0, "Height"))
            else:
                self._width = 0
                self._height = 0

            minfo.Close()

    def filename(self) -> str:
        return self._filename

    def duration(self) -> int:
        return self._duration

    def duration_tuple(self) -> Tuple[int, int, int]:
        # FIXME: use divmod()
        duration = self._duration
        hours = duration // 1000 // 60 // 60
        duration -= 1000 * 60 * 60 * hours
        minutes = duration // 1000 // 60
        duration -= 1000 * 60 * minutes
        seconds = duration // 1000

        return (hours, minutes, seconds)

    def framerate(self) -> float:
        return self._framerate

    def width(self) -> int:
        return self._width

    def height(self) -> int:
        return self._height

    def filesize(self) -> int:
        return self._filesize


# EOF #
