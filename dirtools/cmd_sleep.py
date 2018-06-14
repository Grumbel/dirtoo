# dirtool.py - diff tool for directories
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


from typing import List, Tuple

import argparse
import sys
import os
import time

from dirtools.format import progressbar


def parse_args(args: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Sleep with progressbar")
    parser.add_argument("TIME", nargs=1, type=str2seconds)
    parser.add_argument('-q', '--quiet', action='store_true', default=False,
                        help="Don't show the progressbar")
    parser.add_argument('-c', '--countdown', action='store_true', default=False,
                        help="Count down instead of up")
    return parser.parse_args(args)


def split_duration(duration: int) -> Tuple[int, int, int]:
    rest = duration
    hours, rest = divmod(rest, 1000 * 60 * 60)
    minutes, rest = divmod(rest, 1000 * 60)
    seconds, rest = divmod(rest, 1000)

    return (hours, minutes, seconds)


def fmt_duration(duration: int) -> str:
    hours, minutes, seconds = split_duration(duration)
    return "{:02d}h:{:02d}m:{:02d}s".format(hours, minutes, seconds)


def print_time(p: float, total: float, countdown: bool):
    columns, lines = os.get_terminal_size()

    if countdown:
        lhs, rhs = total - p, total
    else:
        lhs, rhs = p, total

    sys.stdout.write("\r{} / {}  |{}|".format(
        fmt_duration(int(lhs * 1000)),
        fmt_duration(int(rhs * 1000)),
        progressbar(columns - 32, int(p * 1000), int(total * 1000))))


def str2seconds(text: str) -> float:
    if text == "":
        return 0

    text = text.strip()

    if text[-1] == "s":
        return float(text[:-1])
    elif text[-1] == "m":
        return float(text[:-1]) * 60
    elif text[-1] == "h":
        return float(text[:-1]) * 60 * 60
    else:
        return float(text)


def main(argv: List[str]) -> None:
    args = parse_args(argv[1:])

    total = args.TIME[0]

    current = time.time()
    start = current
    end = current + total

    while True:
        current = time.time()
        if current >= end:
            break

        p = current - start
        print_time(p, total, args.countdown)

        time.sleep(0.1)

    print_time(total, total, args.countdown)
    print()


def main_entrypoint() -> None:
    main(sys.argv)


# EOF #
