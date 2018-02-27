#!/usr/bin/env python3

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


from typing import List

import sys
from inotify_simple import INotify, flags, masks


def main(argv: List[str]) -> None:
    inotify = INotify()
    watch_flags = masks.ALL_EVENTS

    print("watching /tmp")
    inotify.add_watch('/tmp', watch_flags)

    while True:
        for event in inotify.read():
            print(event)
            evs = [str(fl) for fl in flags.from_mask(event.mask)]
            print("    " + ", ".join(evs))


if __name__ == "__main__":
    main(sys.argv)


# EOF #
