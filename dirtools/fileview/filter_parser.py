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


import re
import datetime
import operator

import bytefmt


VIDEO_EXT = ['wmv', 'mp4', 'mpg', 'mpeg', 'm2v', 'avi', 'flv', 'mkv', 'wmv',
             'mov', 'webm', 'f4v', 'flv', 'divx', 'ogv', 'vob', '3gp', '3g2',
             'qt', 'asf', 'amv', 'm4v']

VIDEO_REGEX = r"\.({})$".format("|".join(VIDEO_EXT))


IMAGE_EXT = ['jpg', 'jpeg', 'gif', 'png', 'tif', 'tiff', 'webp']

IMAGE_REGEX = r"\.({})$".format("|".join(IMAGE_EXT))


ARCHIVE_EXT = ['zip', 'rar', 'tar', 'gz', 'xz', 'bz2', 'ar', '7z']

ARCHIVE_REGEX = r"\.({})$".format("|".join(ARCHIVE_EXT))

CMPTEXT2OP = {
    "<": operator.lt,
    "<=": operator.le,
    ">": operator.gt,
    ">=": operator.ge,
    "==": operator.eq,
    "=": operator.eq,
}


def get_compare_operator(text):
    return CMPTEXT2OP[text]


class FilterParser:

    def __init__(self, filter):
        self.filter = filter

    def parse(self, pattern):
        if pattern == "":
            self.filter.set_none()
        elif pattern.startswith("/"):
            command, arg = pattern[1:].split("/", 1)
            if command in ["video", "videos", "vid", "vids"]:
                self.filter.set_regex_pattern(VIDEO_REGEX, re.IGNORECASE)
            elif command in ["image", "images", "img", "imgs"]:
                self.filter.set_regex_pattern(IMAGE_REGEX, re.IGNORECASE)
            elif command in ["archive", "archives", "arch", "ar"]:
                self.filter.set_regex_pattern(ARCHIVE_REGEX, re.IGNORECASE)
            elif command in ["r", "rx", "re", "regex"]:
                self.filter.set_regex_pattern(arg, re.IGNORECASE)
            elif command == "today":
                self.filter.set_time(datetime.datetime.combine(
                    datetime.date.today(), datetime.datetime.min.time()).timestamp(),
                    operator.ge)
            elif command.startswith("len"):
                m = re.match(r"len(<|>|<=|>=|=|==)(\d+.*)", command)
                if m is None:
                    self.window.show_info("invalid filter command")
                else:
                    self.filter.set_length(int(m.group(2)), get_compare_operator(m.group(1)))
            elif command.startswith("size"):
                m = re.match(r"size(<|>|<=|>=|=|==)(\d+.*)", command)
                if m is None:
                    self.window.show_info("invalid filter command")
                else:
                    self.filter.set_size(bytefmt.dehumanize(m.group(2)),
                                         get_compare_operator(m.group(1)))
            elif command.startswith("random"):
                m = re.match(r"random(\d*\.\d+)", command)
                if m is None:
                    self.window.show_info("invalid filter command")
                else:
                    self.filter.set_random(float(m.group(1)))
            elif command.startswith("pick"):
                m = re.match(r"pick(\d+)", command)
                if m is None:
                    self.window.show_info("invalid filter command")
                else:
                    self.filter.set_random_pick(int(m.group(1)))
            elif command == "G":
                self.filter.set_pattern(arg, case_sensitive=True)
            elif command == "g":
                self.filter.set_pattern(arg, case_sensitive=False)
            else:
                print("Controller.set_filter: unknown command: {}".format(command))
        else:
            self.filter.set_pattern(pattern, case_sensitive=False)


# EOF #
