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


from typing import Sequence, Tuple

import argparse
import sys
import os
import time

from dirtoo.format import progressbar
import dirtoo.duration as duration


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Sleep with progressbar")
    parser.add_argument("TIME", nargs=1, type=parse_duration)
    parser.add_argument('-q', '--quiet', action='store_true', default=False,
                        help="Don't show the progressbar")
    parser.add_argument('-c', '--countdown', action='store_true', default=False,
                        help="Count down instead of up")
    parser.add_argument('-s', '--start', metavar="SECONDS", type=parse_duration, default=0.0,
                        help="Start the countdown at SECONDS")
    return parser.parse_args(argv[1:])


def split_duration(duration: int) -> Tuple[int, int, int]:
    rest = duration
    hours, rest = divmod(rest, 1000 * 60 * 60)
    minutes, rest = divmod(rest, 1000 * 60)
    seconds, rest = divmod(rest, 1000)

    return (hours, minutes, seconds)


def parse_duration(text: str) -> float:
    result = duration.dehumanize(text)
    if result is None:
        return 0
    else:
        return result


def print_time(p: float, total: float, countdown: bool) -> None:
    columns, lines = os.get_terminal_size()

    if countdown:
        lhs, rhs = total - p, total
    else:
        lhs, rhs = p, total

    sys.stdout.write("\r{} / {}  |{}|".format(
        duration.humanize(int(lhs * 1000)),
        duration.humanize(int(rhs * 1000)),
        progressbar(columns - 24, int(p * 1000), int(total * 1000))))


def main(argv: Sequence[str]) -> None:
    args = parse_args(argv)

    total = args.TIME[0]

    current = time.time() - args.start
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
