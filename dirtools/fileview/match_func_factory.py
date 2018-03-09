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

from dirtools.duration import dehumanize as dehumanize_duration
from dirtools.util import is_glob_pattern
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
    m = re.match(r"^(<=|<|>=|>|==|=)(.*)$", text)
    if m:
        return get_compare_operator(m.group(1)), m.group(2)
    else:
        return operator.eq, text


class MatchFuncFactory:

    def __init__(self):
        self._functions = {}
        self._docs = []
        self._register_defaults()

    def get_docs(self):
        return self._docs

    def register_function(self, aliases: List[str], func: Callable,
                          doc=Optional[str]):
        self._docs.append((aliases, doc))

        for alias in aliases:
            if alias in self._functions:
                logger.error("function name '%s' already in use, overriding", alias)

            self._functions[alias] = func

    def make_match_func(self, child):
        from dirtools.fileview.filter_expr_parser import CommandExpr

        if isinstance(child, str):
            # If the pattern doesn't contain special characters
            # perform a basic substring search instead of a glob
            # pattern search.
            if is_glob_pattern(child):
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
        self.register_function(["charset", "encoding"], self.make_charset,
                               """\
                               {CHARSET}

                               True if the filename is only made of of characters contained in CHARSET.

                               Example: 'charset:ascii'
                               """)

        self.register_function(["contains"], self.make_contains,
                               """\
                               {TEXT}

                               True if the file contains the string TEXT, case-insensitive.

                               Example: 'contains:main()'
                               """)

        self.register_function(["Contains"], self.make_Contains,
                               """\
                               {TEXT}

                               True if the file contains the string TEXT, case-sensitive.

                               Example: 'contains:QApplication()'
                               """)

        self.register_function(["date"], self.make_date,
                               """\
                               {DATEPATTERN}, {CMP}{DATE}

                               Example: 'date:>2017-12', 'date:*-12-24'
                               """)

        self.register_function(["duration"], self.make_duration,
                               """\
                               {CMP}{DURATION}

                               Example: 'duration:>100'
                               """)

        self.register_function(["filecount"], self.make_filecount,
                               """\
                               {CMP}{COUNT}

                               Compares the number files in an archive
                               or directory againt COUNT.

                               Example: 'filecount:>100'
                               """)

        self.register_function(["framerate"], self.make_framerate,
                               """\
                               {CMP}{FRAMERATE}

                               Example: 'framerate:>30'
                               """)

        self.register_function(["fuzzy", "f", "fuz", "fuzz"], self.make_fuzzy,
                               """\
                               {TEXT}

                               Searches for TEXT in the filename using
                               an n-gram fuzzy algorithm that will
                               match similar strings, not just
                               identical ones.

                               Example: 'fuzzy:SpelingMistake'
                               """)

        self.register_function(["glob", "g"], self.make_glob,
                               """\
                               {PATTERN}

                               True if the given glob pattern matches
                               the filename, case-insensitive.

                               Example: 'glob:*.png'
                               """)

        self.register_function(["Glob", "G"], self.make_Glob,
                               """\
                               {PATTERN}

                               True if the given glob pattern matches
                               the filename, case-sensitive.

                               Example: 'glob:*.png'
                               """)

        self.register_function(["height"], self.make_height,
                               """\
                               {CMP}{WIDTH}
                               Matches the height of a video or image.

                               Example: 'height:=480'
                               """)

        self.register_function(["length", "len"], self.make_length,
                               """\
                               {CMP}{COUNT}

                               Matches the number of characters againt
                               COUNT.

                               Example: 'length:>100'
                               """)

        self.register_function(["pages"], self.make_pages,
                               """\
                               {CMP}{PAGECOUNT}
                               Matches the number of pages in a .pdf document.

                               Example: 'pages:>100'
                               """)

        self.register_function(["pick"], self.make_pick,
                               """\
                               {COUNT}

                               Randomly picks COUNT number of items.

                               Example: pick:10
                               """)

        self.register_function(["random"], self.make_random,
                               """\
                               {PROBABILITY}

                               Randomly picks an item with
                               PROBABILITY.

                               Example: random:0.5
                               """)

        self.register_function(["regex", "r", "rx", "re"], self.make_regex,
                               """\
                               {REGEX}

                               Matches filename against the given
                               REGEX, case-insensitive.

                               Example: 'regex:^foo.*\\.jpg$'
                               """)

        self.register_function(["Regex", "R", "Rx", "Re"], self.make_Regex,
                               """\
                               {REGEX}

                               Matches filename against the given
                               REGEX, case-sensitive.

                               Example: 'regex:^Foo.*\\.jpg$'
                               """)

        self.register_function(["size"], self.make_size,
                               """\
                               {CMP}{BYTE}{UNIT}

                               Matches the byte size of the file
                               against the given value. Units are
                               supported.

                               Example: 'size:>5MB'
                               """)

        self.register_function(["time"], self.make_time,
                               """\
                               {TIMEPATTERN}, {CMP}{TIME}

                               Matches the files mtime against the
                               given time

                               Example: 'date:>15:00', 'time:*:30'
                               """)

        self.register_function(["type", "t"], self.make_type,
                               """\
                               {TYPE}

                               Matches the type of the file, valid
                               types are 'image', 'video', 'archive'
                               and 'directory'.

                               Example: 'type:video'
                               """)

        self.register_function(["width"], self.make_width,
                               """\
                               {CMP}{WIDTH}
                               Matches the width of a video or image.

                               Example: 'width:=640'
                               """)

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
            logger.error("unknown type: %s", argument)
            return FalseMatchFunc()

    def make_fuzzy(self, argument):
        return FuzzyMatchFunc(argument)

    def make_date(self, argument):
        if argument == "today":
            return DateOpMatchFunc(datetime.date.today().strftime("%Y-%m-%d"),
                                   operator.ge)
        else:
            if is_glob_pattern(argument):
                return DateMatchFunc(argument)
            else:
                op, rest = parse_op(argument)
                return DateOpMatchFunc(rest, op)

    def make_time(self, argument):
        if is_glob_pattern(argument):
            return TimeMatchFunc(argument)
        else:
            op, rest = parse_op(argument)
            return TimeOpMatchFunc(rest, op)

    def make_charset(self, argument):
        if argument == "ascii":
            return AsciiMatchFunc()
        else:
            logger.error("unknown charset in command: %s", argument)
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
        seconds = dehumanize_duration(rest)

        if seconds is None:
            logger.error("can't parse duration: %s", argument)
            return FalseMatchFunc()
        else:
            return MetadataMatchFunc("duration", float, 1000 * seconds, op)

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
