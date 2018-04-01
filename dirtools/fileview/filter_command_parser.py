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


from typing import Any, Dict, Callable, List, Union, Tuple, Optional

import logging
import operator
import shlex

logger = logging.getLogger(__name__)


VIDEO_EXT = ['wmv', 'mp4', 'mpg', 'mpeg', 'm2v', 'avi', 'flv', 'mkv', 'wmv',
             'mov', 'webm', 'f4v', 'flv', 'divx', 'ogv', 'vob', '3gp', '3g2',
             'qt', 'asf', 'amv', 'm4v']

VIDEO_REGEX = r"\.({})$".format("|".join(VIDEO_EXT))


IMAGE_EXT = ['jpg', 'jpeg', 'gif', 'png', 'tif', 'tiff', 'webp', 'bmp', 'xcf']

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


def get_compare_operator(text: str) -> Callable[[Any, Any], bool]:
    return CMPTEXT2OP[text]


class FilterCommandParser:

    def __init__(self) -> None:
        self._commands: Dict[str, Tuple[List[str], Callable, str]] = {}
        self._register_commands()

    def parse(self, pattern):
        command, *rest = pattern[1:].split(" ", 1)
        args = shlex.split(rest[0]) if rest else []
        print("Args:", args)
        cmd = self._commands.get(command, None)
        if cmd is None:
            print("Controller.set_filter: unknown command: {}".format(command))
        else:
            _, func, _ = cmd
            func(args)

    def register_command(self,
                         aliases: Union[str, List[str]],
                         func: Callable,
                         help: Optional[str] = None) -> None:
        if isinstance(aliases, list):
            for name in aliases:
                assert name not in self._commands
                self._commands[name] = (aliases, func, help)
        else:
            self._commands[aliases] = ([aliases], func, help)

    def _register_commands(self):
        pass
        # self.register_command(
        #     ["video", "videos", "vid", "vids"],
        #     lambda args: self._filter.set_regex_pattern(VIDEO_REGEX, re.IGNORECASE))

        # self.register_command(
        #     ["image", "images", "img", "imgs"],
        #     lambda args: self._filter.set_regex_pattern(IMAGE_REGEX, re.IGNORECASE))

        # self.register_command(
        #     ["archive", "archives", "arch", "ar"],
        #     lambda args: self._filter.set_regex_pattern(ARCHIVE_REGEX, re.IGNORECASE))

        # self.register_command(
        #     ["r", "rx", "re", "regex"],
        #     lambda args: self._filter.set_regex_pattern(args[0], re.IGNORECASE))

        # self.register_command(
        #     "len",
        #     lambda args: self._filter.set_length(int(args[1]), get_compare_operator(args[0])))

        # self.register_command(
        #     "size",
        #     lambda args: self._filter.set_size(bytefmt.dehumanize(args[1]),
        #                                        get_compare_operator(args[0])))

        # self.register_command(
        #     "random",
        #     lambda args: self._filter.set_random(float(args[0])))

        # self.register_command(
        #     "pick",
        #     lambda args: self._filter.set_random_pick(int(args[0])))

        # self.register_command(
        #     ["folder", "folders", "dir", "dirs", "directories"],
        #     lambda args: self._filter.set_folder())

        # self.register_command(
        #     ["G", "Glob"],
        #     lambda args: self._filter.set_pattern(args[0], case_sensitive=True),
        #     help="Case-sensitive glob matching")

        # self.register_command(
        #     ["g", "glob"],
        #     lambda args: self._filter.set_pattern(args[0], case_sensitive=False),
        #     help="Case-insensitive glob matching")

        # self.register_command(
        #     ["f", "fuz", "fuzz", "fuzzy"],

        #     lambda args: self._filter.set_fuzzy(args[0]),
        #     help="Fuzzy match the filename")

        # self.register_command(
        #     ["ascii"],
        #     lambda args: self._filter.set_ascii(True),
        #     help="filenames with only ASCII character")

        # self.register_command(
        #     ["nonascii"],
        #     lambda args: self._filter.set_ascii(False),
        #     help="filenames with some non-ASCII character")

        # self.register_command(
        #     ["help", "h"],
        #     lambda args: self.print_help())


# EOF #
