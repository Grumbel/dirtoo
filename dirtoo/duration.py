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


from typing import Tuple, Optional

import re


def split(duration_sec: int) -> Tuple[int, int, int]:
    rest = duration_sec
    hours, rest = divmod(rest, 1000 * 60 * 60)
    minutes, rest = divmod(rest, 1000 * 60)
    seconds = rest // 1000

    return (hours, minutes, seconds)


def total_seconds(hours: int, minutes: int, seconds: int) -> int:
    return 60 * 60 * hours + 60 * minutes + seconds


def humanize(duration_sec: int) -> str:
    hours, minutes, seconds = split(duration_sec)
    return "{:d}:{:02d}:{:02d}".format(hours, minutes, seconds)


TIME_HMS_RX = re.compile(r'^(?:(\d+)h)?(?:(\d+)m)?(?:(\d+)s)?$')


def dehumanize_hms(text: str) -> Optional[int]:
    m = TIME_HMS_RX.match(text)
    if m is None:
        return None
    else:
        hours = int(m.group(1) or 0)
        minutes = int(m.group(2) or 0)
        seconds = int(m.group(3) or 0)

        return total_seconds(hours, minutes, seconds)


TIME_DOT_RX = re.compile(r'^(\d+):(\d+)(?::(\d+)|([hm]))?$', re.IGNORECASE)


def dehumanize_dot(text: str) -> Optional[int]:
    m = TIME_DOT_RX.match(text)
    if m is None:
        return None
    else:
        if m.group(3) is None:
            if m.group(4) is None:
                hours = 0
                minutes = int(m.group(1))
                seconds = int(m.group(2))
            elif m.group(4).lower() == "m":
                hours = 0
                minutes = int(m.group(1))
                seconds = int(m.group(2))
            elif m.group(4).lower() == "h":
                hours = int(m.group(1))
                minutes = int(m.group(2))
                seconds = 0
            return total_seconds(hours, minutes, seconds)
        elif m.group(4) is None:
            hours = int(m.group(1))
            minutes = int(m.group(2))
            seconds = int(m.group(3))
            return total_seconds(hours, minutes, seconds)
        else:
            return None


TIME_UNIT_RX = re.compile(r'^(\d+|\d+\.\d*|\d*\.\d+)([hms]?)$', re.IGNORECASE)


def dehumanize_unit(text: str) -> Optional[int]:
    m = TIME_UNIT_RX.match(text)
    if m is None:
        return None
    else:
        value = float(m.group(1))
        unit = m.group(2).lower()
        if unit == "h":
            return int(60 * 60 * value)
        elif unit == "m":
            return int(60 * value)
        elif unit in ["s", ""]:
            return int(value)
        else:
            return None


def dehumanize(text: str) -> Optional[int]:
    return (dehumanize_hms(text) or
            dehumanize_dot(text) or
            dehumanize_unit(text))


# EOF #
