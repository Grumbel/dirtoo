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

import pymediainfo


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

        minfo = pymediainfo.MediaInfo.parse(filename)

        self._general_count = 0
        self._video_count = 0
        self._audio_count = 0
        self._image_count = 0

        for track in minfo.tracks:
            if track.track_type == "General":
                self._general_count += 1
            elif track.track_type == "Video":
                self._video_count += 1
            elif track.track_type == "Audio":
                self._audio_count += 1
            elif track.track_type == "Image":
                self._image_count += 1
            else:
                raise Exception("unknown track type: {}".format(track.track_type))

            # print(repr(minfo.Get(MediaInfoDLL3.Stream.Video, 0, "FrameRate_Mode/String"))) # CFR VFR
            # print(repr(minfo.Get(MediaInfoDLL3.Stream.Video, 0, "FrameRate_Maximum")))
            # print(repr(minfo.Get(MediaInfoDLL3.Stream.Video, 0, "FrameRate_Minimum/String")))

            if track.track_type == "General" and self._general_count == 1:
                self._duration = _to_int(track.duration)
                self._framerate = _to_float(track.frame_rate)
                self._filesize = _to_int(track.file_size)
                # self._bitrate = minfo.Get(MediaInfoDLL3.Stream.General, 0, "OverallBitRate")

            if track.track_type == "Video" and self._video_count == 1:
                self._width = track.width
                self._height = track.height
            elif track.track_type == "Image" and self._image_count == 1:
                self._width = track.width
                self._height = track.height
            else:
                self._width = 0
                self._height = 0

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
