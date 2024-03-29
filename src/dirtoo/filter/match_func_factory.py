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


from typing import TYPE_CHECKING, Sequence, Callable, Tuple, Any, Dict, Union

import datetime
import logging
import operator
import re

import bytefmt

import dirtoo.duration as duration
import dirtoo.file_type as file_type
from dirtoo.fuzzy import fuzzy
from dirtoo.glob import is_glob_pattern
from dirtoo.filter.match_func import (
    MatchFunc,
    FalseMatchFunc,
    ExcludeMatchFunc,
    RegexMatchFunc,
    GlobMatchFunc,
    FuzzyMatchFunc,
    SizeMatchFunc,
    LengthMatchFunc,
    RandomMatchFunc,
    FolderMatchFunc,
    CharsetMatchFunc,
    MetadataMatchFunc,
    ContainsMatchFunc,
    DateMatchFunc,
    TimeMatchFunc,
    DateOpMatchFunc,
    TimeOpMatchFunc,
    WeekdayMatchFunc,
)

if TYPE_CHECKING:
    from dirtoo.filter.filter_expr_parser import CommandExpr


logger = logging.getLogger(__name__)


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

    def __init__(self) -> None:
        self._functions: Dict[str, Callable[[str], MatchFunc]] = {}
        self._docs: list[Tuple[Sequence[str], str]] = []
        self._register_defaults()

    def get_docs(self) -> Sequence[Tuple[Sequence[str], str]]:
        return self._docs

    def register_function(self, aliases: Sequence[str], func: Callable[[str], MatchFunc], doc: str = "") -> None:
        self._docs.append((aliases, doc))

        for alias in aliases:
            if alias in self._functions:
                logger.error("function name '%s' already in use, overriding", alias)

            self._functions[alias] = func

    def make_match_func(self, child: Union[str, 'CommandExpr']) -> MatchFunc:
        from dirtoo.filter.filter_expr_parser import CommandExpr

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

    def _register_defaults(self) -> None:
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

        self.register_function(["containsre", "containsrx", "containsregex"],
                               self.make_contains_regex,
                               """\
                               {REGEX}

                               True if the file contains the regex REGEX, case-insensitive.

                               Example: 'contains:main(.*)'
                               """)

        self.register_function(["Containsre", "Containsrx", "ContainsRe", "ContainsRx", "ContainsRegex"],
                               self.make_Contains_Regex,
                               """\
                               {REGEX}

                               True if the file contains the regex REGEX, case-sensitive.

                               Example: 'contains:QApplication(.*)'
                               """)

        self.register_function(["containsfuzzy", "containsfuz", "containsfuzz"], self.make_contains_fuzzy,
                               """\
                               {FUZZYPATTERN}

                               True if the file contains the string
                               FUZZYPATTERN or something similar,
                               case-insensitive.

                               Example: 'containsfuzzy:main()'
                               """)

        self.register_function(["date"], self.make_date,
                               """\
                               {DATEPATTERN}, {CMP}{DATE}

                               Example: 'date:>2017-12', 'date:*-12-24'
                               """)

        self.register_function(["weekday"], self.make_weekday,
                               """\
                               {WEEKDAY}, {CMP}{WEEKDAY}

                               Example: 'date:>friday', 'date:monday'
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

        # self.register_function(["pick"], self.make_pick,
        #                        """\
        #                        {COUNT}

        #                        Randomly picks COUNT number of items.

        #                        Example: pick:10
        #                        """)

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

    def make_glob(self, argument: str) -> GlobMatchFunc:
        return GlobMatchFunc(argument, case_sensitive=False)

    def make_Glob(self, argument: str) -> GlobMatchFunc:
        return GlobMatchFunc(argument, case_sensitive=True)

    def make_type(self, argument: str) -> MatchFunc:
        if argument == "video":
            return RegexMatchFunc(file_type.VIDEO_REGEX, re.IGNORECASE)
        elif argument == "image":
            return RegexMatchFunc(file_type.IMAGE_REGEX, re.IGNORECASE)
        elif argument == "archive":
            return RegexMatchFunc(file_type.ARCHIVE_REGEX, re.IGNORECASE)
        elif argument in ["folder", "dir", "directory"]:
            return FolderMatchFunc()
        elif argument in ["file"]:
            return ExcludeMatchFunc(FolderMatchFunc())
        else:
            logger.error("unknown type: %s", argument)
            return FalseMatchFunc()

    def make_fuzzy(self, argument: str) -> FuzzyMatchFunc:
        return FuzzyMatchFunc(argument)

    def make_date(self, argument: str) -> MatchFunc:
        if argument == "today":
            return DateOpMatchFunc(datetime.date.today().strftime("%Y-%m-%d"),
                                   operator.ge)
        else:
            if is_glob_pattern(argument):
                return DateMatchFunc(argument)
            else:
                op, rest = parse_op(argument)
                return DateOpMatchFunc(rest, op)

    def make_weekday(self, argument: str) -> MatchFunc:
        op, rest = parse_op(argument)
        return WeekdayMatchFunc(rest, op)

    def make_time(self, argument: str) -> MatchFunc:
        if is_glob_pattern(argument):
            return TimeMatchFunc(argument)
        else:
            op, rest = parse_op(argument)
            return TimeOpMatchFunc(rest, op)

    def make_charset(self, argument: str) -> MatchFunc:
        try:
            "".encode(argument)  # test if argument is a valid charset
            return CharsetMatchFunc(argument)
        except LookupError as err:
            logger.error("unknown charset in command: %s: %s", argument, err)
            return FalseMatchFunc()

    def make_contains(self, argument: str) -> MatchFunc:
        needle = argument.lower()

        def line_match_func(line: str, needle: str = needle) -> bool:
            return needle in line.lower()

        return ContainsMatchFunc(line_match_func)

    def make_Contains(self, argument: str) -> MatchFunc:
        needle = argument

        def line_match_func(line: str, needle: str = needle) -> bool:
            return needle in line

        return ContainsMatchFunc(line_match_func)

    def make_contains_regex(self, argument: str) -> MatchFunc:
        rx = re.compile(argument, re.IGNORECASE)

        def line_match_func(line: str, rx: re.Pattern[str] = rx) -> bool:
            return bool(rx.search(line))

        return ContainsMatchFunc(line_match_func)

    def make_Contains_Regex(self, argument: str) -> MatchFunc:
        rx = re.compile(argument)

        def line_match_func(line: str, rx: re.Pattern[str] = rx) -> bool:
            return bool(rx.search(line))

        return ContainsMatchFunc(line_match_func)

    def make_contains_fuzzy(self, argument: str) -> MatchFunc:
        needle = argument
        n = 3
        threshold = 0.5

        def line_match_func(line: str, needle: str = needle, n: int = n, threshold: float = threshold) -> bool:
            return fuzzy(needle, line, n) > threshold

        return ContainsMatchFunc(line_match_func)

    def make_regex(self, argument: str) -> MatchFunc:
        return RegexMatchFunc(argument, re.IGNORECASE)

    def make_Regex(self, argument: str) -> RegexMatchFunc:
        return RegexMatchFunc(argument, re.RegexFlag(0))

    def make_length(self, argument: str) -> LengthMatchFunc:
        op, rest = parse_op(argument)
        return LengthMatchFunc(int(rest), op)

    def make_size(self, argument: str) -> SizeMatchFunc:
        op, rest = parse_op(argument)
        return SizeMatchFunc(bytefmt.dehumanize(rest), op)

    def make_random(self, argument: str) -> RandomMatchFunc:
        return RandomMatchFunc(float(argument))

    def make_duration(self, argument: str) -> MatchFunc:
        op, rest = parse_op(argument)
        seconds = duration.dehumanize(rest)

        if seconds is None:
            logger.error("can't parse duration: %s", argument)
            return FalseMatchFunc()
        else:
            return MetadataMatchFunc("duration", float, 1000 * seconds, op)

    def make_width(self, argument: str) -> MatchFunc:
        op, rest = parse_op(argument)
        return MetadataMatchFunc("width", float, float(rest), op)

    def make_height(self, argument: str) -> MetadataMatchFunc:
        op, rest = parse_op(argument)
        return MetadataMatchFunc("height", float, float(rest), op)

    def make_framerate(self, argument: str) -> MetadataMatchFunc:
        op, rest = parse_op(argument)
        return MetadataMatchFunc("framerate", float, float(rest), op)

    def make_pages(self, argument: str) -> MetadataMatchFunc:
        op, rest = parse_op(argument)
        return MetadataMatchFunc("pages", int, int(rest), op)

    def make_filecount(self, argument: str) -> MetadataMatchFunc:
        op, rest = parse_op(argument)
        return MetadataMatchFunc("file_count", int, int(rest), op)


# EOF #
