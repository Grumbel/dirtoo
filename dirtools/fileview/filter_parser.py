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


from typing import Dict, Callable, List, Union, Tuple, Optional

import re
import sys
import datetime
import operator

import bytefmt


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


def get_compare_operator(text):
    return CMPTEXT2OP[text]


class FilterParser:

    def __init__(self, filter):
        self.filter = filter
        self.grammar = self._make_grammar()
        self.commands: Dict[str, Tuple[List[str], Callable, str]] = {}
        self.reg_commands: List[Tuple[List[str], Callable, str]] = []
        self._register_commands()

    def _make_grammar(self):
        from pyparsing import (QuotedString, ZeroOrMore, Combine, OneOrMore, Regex)

        def escape_handler(s, loc, toks):
            if toks[0] == '\\\\':
                return "\\"
            elif toks[0] == '\\\'':
                return "'"
            elif toks[0] == '\\"':
                return '"'
            elif toks[0] == '\\f':
                return "\f"
            elif toks[0] == '\\n':
                return "\n"
            elif toks[0] == '\\r':
                return "\r"
            elif toks[0] == '\\t':
                return "\t"
            elif toks[0] == '\\ ':
                return " "
            else:
                return toks[0][1:]

        escape = Combine(Regex(r'\\.')).setParseAction(escape_handler)
        word = Combine(OneOrMore(escape | Regex(r'[^\s\\]+')))
        arguments = ZeroOrMore(QuotedString('"', escChar='\\') | QuotedString("'", escChar='\\') | word)
        return arguments

    def _register_commands(self):
        self.register_command(
            ["video", "videos", "vid", "vids"],
            lambda args: self.filter.set_regex_pattern(VIDEO_REGEX, re.IGNORECASE))

        self.register_command(
            ["image", "images", "img", "imgs"],
            lambda args: self.filter.set_regex_pattern(IMAGE_REGEX, re.IGNORECASE))

        self.register_command(
            ["archive", "archives", "arch", "ar"],
            lambda args: self.filter.set_regex_pattern(ARCHIVE_REGEX, re.IGNORECASE))

        self.register_command(
            ["r", "rx", "re", "regex"],
            lambda args: self.filter.set_regex_pattern(args[0], re.IGNORECASE))

        self.register_command(
            "today",
            lambda args: self.filter.set_time(
                datetime.datetime.combine(
                    datetime.date.today(), datetime.datetime.min.time()).timestamp(),
                operator.ge))

        self.register_command(
            "len",
            lambda args: self.filter.set_length(int(args[1]), get_compare_operator(args[0])))

        self.register_command(
            "size",
            lambda args: self.filter.set_size(bytefmt.dehumanize(args[1]),
                                              get_compare_operator(args[0])))

        self.register_command(
            "random",
            lambda args: self.filter.set_random(float(args[0])))

        self.register_command(
            "pick",
            lambda args: self.filter.set_random_pick(int(args[0])))

        self.register_command(
            ["folder", "folders", "dir", "dirs", "directories"],
            lambda args: self.filter.set_folder())

        self.register_command(
            ["G", "Glob"],
            lambda args: self.filter.set_pattern(args[0], case_sensitive=True),
            help="Case-sensitive glob matching")

        self.register_command(
            ["g", "glob"],
            lambda args: self.filter.set_pattern(args[0], case_sensitive=False),
            help="Case-insensitive glob matching")

        self.register_command(
            ["f", "fuz", "fuzz", "fuzzy"],
            lambda args: self.filter.set_fuzzy(args[0]),
            help="Fuzzy match the filename")

        self.register_command(
            ["ascii"],
            lambda args: self.filter.set_ascii(True),
            help="filenames with only ASCII character")

        self.register_command(
            ["nonascii"],
            lambda args: self.filter.set_ascii(False),
            help="filenames with some non-ASCII character")

        self.register_command(
            ["help", "h"],
            lambda args: self.print_help())

    def print_help(self, fout=sys.stdout):
        for aliases, func, help in self.reg_commands:
            print("{}".format(", ".join(["/" + x for x in aliases])), file=fout)
            print("  {}".format(help or ""), file=fout)
            print(file=fout)

    def register_command(self,
                         aliases: Union[str, List[str]],
                         func: Callable,
                         help: Optional[str]=None) -> None:
        if isinstance(aliases, list):
            for name in aliases:
                assert name not in self.commands
                self.commands[name] = (aliases, func, help)
            self.reg_commands.append((aliases, func, help))
        else:
            self.commands[aliases] = ([aliases], func, help)
            self.reg_commands.append(([aliases], func, help))

    def parse(self, pattern):
        if pattern == "":
            self.filter.set_none()
        elif pattern.startswith("/"):
            command, *rest = pattern[1:].split(" ", 1)
            args = self.grammar.parseString(rest[0], parseAll=True) if rest else []
            print("Args:", args)
            cmd = self.commands.get(command, None)
            if cmd is None:
                print("Controller.set_filter: unknown command: {}".format(command))
            else:
                _, func, _ = cmd
                func(args)
        else:
            # If the pattern doesn't contain special characters
            # perform a basic substring search instead of a glob
            # pattern search.
            if re.search(r"[\*\?\[\]]", pattern):
                self.filter.set_pattern(pattern, case_sensitive=False)
            else:
                self.filter.set_pattern("*{}*".format(pattern), case_sensitive=False)


# EOF #
