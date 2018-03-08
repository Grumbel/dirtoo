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


from typing import List, Callable, Optional, Tuple, Any


import datetime
import bytefmt
import operator
import logging
import re

from dirtools.fileview.match_func import (
    FalseMatchFunc,
    ExcludeMatchFunc,
    RegexMatchFunc,
    GlobMatchFunc,
    FuzzyMatchFunc,
    SizeMatchFunc,
    LengthMatchFunc,
    RandomMatchFunc,
    RandomPickMatchFunc,
    FolderMatchFunc,
    AsciiMatchFunc,
    MetadataMatchFunc,
    ContainsMatchFunc,
    DateMatchFunc,
    TimeMatchFunc,
    DateOpMatchFunc,
    TimeOpMatchFunc,
)

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


def parse_op(text: str) -> Tuple[Callable[[Any, Any], bool], str]:
    m = re.match(r"^(<|<=|>|>=|==|=)(.*)$", text)
    if m:
        return get_compare_operator(m.group(1)), m.group(2)
    else:
        return operator.eq, text


class MatchFuncFactory:

    def __init__(self):
        self._functions = {}
        self._register_defaults()

    def register_function(self, aliases: List[str], func: Callable, doc=Optional[str]):
        for alias in aliases:
            self._functions[alias] = func

    def make_match_func(self, child):
        from dirtools.fileview.filter_expr_parser import CommandExpr

        if isinstance(child, str):
            # If the pattern doesn't contain special characters
            # perform a basic substring search instead of a glob
            # pattern search.
            if re.search(r"[\*\?\[\]]", child):
                return GlobMatchFunc(child, case_sensitive=False)
            else:
                return GlobMatchFunc(f"*{child}*", case_sensitive=False)
        elif isinstance(child, CommandExpr):
            func = self._functions.get(child.command, None)
            if func is not None:
                return func(child.arg)
            else:
                logger.error("unknown filter command: %s", child)
                return FalseMatchFunc()
        else:
            assert False, "unknown child: {}".format(child)

    def _register_defaults(self):
        self.register_function(["g", "glob"], self.make_glob)
        self.register_function(["G", "Glob"], self.make_Glob)
        self.register_function(["contains"], self.make_contains)
        self.register_function(["Contains"], self.make_Contains)
        self.register_function(["t", "type"], self.make_type)
        self.register_function(["f", "fuz", "fuzz", "fuzzy"], self.make_fuzzy)
        self.register_function(["duration"], self.make_duration)
        self.register_function(["framerate"], self.make_framerate)
        self.register_function(["filecount"], self.make_filecount)
        self.register_function(["pages"], self.make_pages)
        self.register_function(["width"], self.make_width)
        self.register_function(["height"], self.make_height)
        self.register_function(["pick"], self.make_pick)
        self.register_function(["random"], self.make_random)
        self.register_function(["size"], self.make_size)
        self.register_function(["len", "length"], self.make_length)
        self.register_function(["r", "rx", "re", "regex"], self.make_regex)
        self.register_function(["R", "Rx", "Re", "Regex"], self.make_Regex)
        self.register_function(["charset", "encoding"], self.make_charset)
        self.register_function(["f", "fuz", "fuzz", "fuzzy"], self.make_fuzzy)
        self.register_function(["date"], self.make_date)

    def make_glob(self, argument):
        return GlobMatchFunc(argument, case_sensitive=False)

    def make_Glob(self, argument):
        return GlobMatchFunc(argument, case_sensitive=True)

    def make_type(self, argument):
        if argument == "video":
            return RegexMatchFunc(VIDEO_REGEX, re.IGNORECASE)
        elif argument == "image":
            return RegexMatchFunc(IMAGE_REGEX, re.IGNORECASE)
        elif argument == "archive":
            return RegexMatchFunc(ARCHIVE_REGEX, re.IGNORECASE)
        elif argument in ["folder", "dir", "directory"]:
            return FolderMatchFunc()
        elif argument in ["file"]:
            return ExcludeMatchFunc(FolderMatchFunc())
        else:
            logger.error("unknown type: %s", child)
            return FalseMatchFunc()

    def make_fuzzy(self, argument):
        return FuzzyMatchFunc(argument)

    def make_date(self, argument):
        if argument == "today":
            return DateOpMatchFunc(datetime.date.today().strftime("%Y-%m-%d"),
                                   operator.ge)
        else:
            op, rest = parse_op(argument)
            if op == operator.eq:
                return DateMatchFunc(rest)
            else:
                return DateOpMatchFunc(rest, op)

    def make_time(self, argument):
        op, rest = parse_op(argument)
        if op == operator.eq:
            return TimeMatchFunc(rest)
        else:
            return TimeOpMatchFunc(rest, op)

    def make_charset(self, argument):
        if argument == "ascii":
            return AsciiMatchFunc()
        else:
            logger.error("unknown charset in command: %s", child)
            return FalseMatchFunc()

    def make_contains(self, argument):
        return ContainsMatchFunc(argument, case_sensitive=False)

    def make_Contains(self, argument):
        return ContainsMatchFunc(argument, case_sensitive=True)

    def make_regex(self, argument):
        return RegexMatchFunc(argument, re.IGNORECASE)

    def make_Regex(self, argument):
        return RegexMatchFunc(argument, 0)

    def make_length(self, argument):
        op, rest = parse_op(argument)
        return LengthMatchFunc(int(rest), op)

    def make_size(self, argument):
        op, rest = parse_op(argument)
        return SizeMatchFunc(bytefmt.dehumanize(rest), op)

    def make_random(self, argument):
        return RandomMatchFunc(float(argument))

    def make_pick(self, argument):
        return RandomPickMatchFunc(int(argument))

    def make_duration(self, argument):
        op, rest = parse_op(argument)
        return MetadataMatchFunc("duration", float, float(rest), op)

    def make_width(self, argument):
        op, rest = parse_op(argument)
        return MetadataMatchFunc("width", float, float(rest), op)

    def make_height(self, argument):
        op, rest = parse_op(argument)
        return MetadataMatchFunc("height", float, float(rest), op)

    def make_framerate(self, argument):
        op, rest = parse_op(argument)
        return MetadataMatchFunc("framerate", float, float(rest), op)

    def make_pages(self, argument):
        op, rest = parse_op(argument)
        return MetadataMatchFunc("pages", int, int(rest), op)

    def make_filecount(self, argument):
        op, rest = parse_op(argument)
        return MetadataMatchFunc("file_count", int, int(rest), op)


# EOF #
