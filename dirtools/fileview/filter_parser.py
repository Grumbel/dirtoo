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

import logging
import re
import sys
import datetime
import operator
import shlex

import bytefmt

from dirtools.fileview.match_func import (
    FalseMatchFunc,
    ExcludeMatchFunc,
    AndMatchFunc,
    OrMatchFunc,
    RegexMatchFunc,
    GlobMatchFunc,
    FuzzyMatchFunc,
    SizeMatchFunc,
    LengthMatchFunc,
    TimeMatchFunc,
    RandomMatchFunc,
    RandomPickMatchFunc,
    FolderMatchFunc,
    AsciiMatchFunc,
    MetadataMatchFunc,
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


def get_compare_operator(text):
    return CMPTEXT2OP[text]


def parse_op(text):
    m = re.match(r"^(<|<=|>|>=|==|=)(.*)$", text)
    if m:
        return get_compare_operator(m.group(1)), m.group(2)
    else:
        return operator.eq, text


class IncludeExpr:

    def __init__(self, s, loc, toks):
        self.child = toks[0]

    def __repr__(self):
        return f"Include({self.child})"


class ExcludeExpr:

    def __init__(self, s, loc, toks):
        self.child = toks[0]

    def __repr__(self):
        return f"Exclude({self.child})"


class CommandExpr:

    def __init__(self, s, loc, toks):
        self.command = toks[0]
        self.arg = toks[1]

    def __repr__(self):
        return f"Command({self.command}:{self.arg})"


class OrKeywordExpr:

    def __init__(self, s, loc, toks):
        self.keyword = toks[0].upper()

    def __repr__(self):
        return f"OR"


class AndKeywordExpr:

    def __init__(self, s, loc, toks):
        self.keyword = toks[0].upper()

    def __repr__(self):
        return f"AND"


class FilterParser:

    def __init__(self, filter):
        self._filter = filter
        self._grammar = self._make_grammar()
        self._commands: Dict[str, Tuple[List[str], Callable, str]] = {}
        self._register_commands()

    def _make_grammar(self):
        from pyparsing import (QuotedString, ZeroOrMore, Combine,
                               Word, Literal, Optional, OneOrMore,
                               Regex, alphas, CaselessKeyword)

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
        whitespace = Regex(r'\s+').suppress()
        quotedstring = Combine(OneOrMore(QuotedString('"', escChar='\\') | QuotedString("'", escChar='\\')))
        command = Word(alphas) + Literal(":").suppress() + (quotedstring | word)
        include = quotedstring | command | word
        exclude = Literal("-").suppress() + (quotedstring | command | word)
        or_keyword = CaselessKeyword("or")
        and_keyword = CaselessKeyword("and")
        keyword = or_keyword | and_keyword

        argument = (keyword | exclude | include)
        expr = ZeroOrMore(Optional(whitespace) + argument)

        # arguments.leaveWhitespace()

        command.setParseAction(CommandExpr)
        include.setParseAction(IncludeExpr)
        exclude.setParseAction(ExcludeExpr)
        or_keyword.setParseAction(OrKeywordExpr)
        and_keyword.setParseAction(AndKeywordExpr)

        # or_expr.setParseAction(lambda s, loc, toks: OrOperator(toks[0], toks[2]))
        # and_expr.setParseAction(lambda s, loc, toks: AndOperator(toks[0], toks[2]))
        # no_expr.setParseAction(lambda s, loc, toks: AndOperator(toks[0], toks[1]))
        # expr.setParseAction(Operator)

        return expr

    def _parse_tokens(self, tokens):
        result = [[]]

        for token in tokens:
            if isinstance(token, AndKeywordExpr):
                pass  # ignore
            elif isinstance(token, OrKeywordExpr):
                result.append([])
            elif (isinstance(token, IncludeExpr) or
                  isinstance(token, ExcludeExpr) or
                  isinstance(token, CommandExpr)):
                result[-1].append(token)
            else:
                assert False, "unknown token: {}".format(token)

        # Remove empty lists that result from unterminated OR keywords
        result = list(filter(bool, result))

        return result

    def parse2(self, text):
        tokens = self._grammar.parseString(text, parseAll=True)
        parsed_tokens = self._parse_tokens(tokens)

        or_funcs = []
        for tokens in parsed_tokens:
            and_funcs = []
            for token in tokens:
                and_funcs.append(self._make_func(token))
            or_funcs.append(AndMatchFunc(and_funcs))

        return OrMatchFunc(or_funcs)

    def _make_child_func(self, child):
        if isinstance(child, str):
            # If the pattern doesn't contain special characters
            # perform a basic substring search instead of a glob
            # pattern search.
            if re.search(r"[\*\?\[\]]", child):
                return GlobMatchFunc(child, case_sensitive=False)
            else:
                return GlobMatchFunc(f"*{child}*", case_sensitive=False)
        elif isinstance(child, CommandExpr):
            if child.command == "glob":
                return GlobMatchFunc(child.arg, case_sensitive=False)
            elif child.command == "Glob":
                return GlobMatchFunc(child.arg, case_sensitive=True)
            elif child.command in ["t", "type"]:
                if child.arg == "video":
                    return RegexMatchFunc(VIDEO_REGEX, re.IGNORECASE)
                elif child.arg == "image":
                    return RegexMatchFunc(IMAGE_REGEX, re.IGNORECASE)
                elif child.arg == "archive":
                    return RegexMatchFunc(ARCHIVE_REGEX, re.IGNORECASE)
                elif child.arg in ["folder", "dir", "directory"]:
                    return FolderMatchFunc()
                elif child.arg in ["file"]:
                    return ExcludeMatchFunc(FolderMatchFunc())
                else:
                    logger.error("unknown type: %s", child)
                    return FalseMatchFunc()
            elif child.command == "fuzzy":
                return FuzzyMatchFunc(child.arg)
            elif child.command == "charset" or child.command == "encoding":
                if child.arg == "ascii":
                    return AsciiMatchFunc()
                else:
                    logger.error("unknown charset in command: %s", child)
                    return FalseMatchFunc()
            elif child.command in ["r", "rx", "re", "regex"]:
                return RegexMatchFunc(child.arg, re.IGNORECASE)
            elif child.command in ["R", "Rx", "Re", "Regex"]:
                return RegexMatchFunc(child.arg, 0)
            elif child.command in ["len", "length"]:
                op, rest = parse_op(child.arg)
                return LengthMatchFunc(int(rest), op)
            elif child.command in ["size"]:
                op, rest = parse_op(child.arg)
                return SizeMatchFunc(bytefmt.dehumanize(rest), op)
            elif child.command in ["date", "time", "mtime"]:
                if child.arg == "today":
                    return TimeMatchFunc(
                        datetime.datetime.combine(
                            datetime.date.today(), datetime.datetime.min.time()).timestamp(),
                        operator.ge)
                else:
                    assert False, "unknown time string: {}".format(child)
            elif child.command == "random":
                return RandomMatchFunc(float(child.arg))
            elif child.command == "pick":
                return RandomPickMatchFunc(int(child.arg))
            elif child.command == "duration":
                op, rest = parse_op(child.arg)
                return MetadataMatchFunc("duration", float, float(rest), op)
            elif child.command == "width":
                op, rest = parse_op(child.arg)
                return MetadataMatchFunc("width", float, float(rest), op)
            elif child.command == "height":
                op, rest = parse_op(child.arg)
                return MetadataMatchFunc("height", float, float(rest), op)
            elif child.command == "framerate":
                op, rest = parse_op(child.arg)
                return MetadataMatchFunc("framerate", float, float(rest), op)
            elif child.command == "pages":
                op, rest = parse_op(child.arg)
                return MetadataMatchFunc("pages", int, int(rest), op)
            elif child.command == "filecount":
                op, rest = parse_op(child.arg)
                return MetadataMatchFunc("file_count", int, int(rest), op)
            else:
                logger.error("unknown filter command: %s", child)
                return FalseMatchFunc()
        else:
            assert False, "unknown child: {}".format(child)

    def _make_func(self, token):
        if isinstance(token, IncludeExpr):
            return self._make_child_func(token.child)
        elif isinstance(token, ExcludeExpr):
            return ExcludeMatchFunc(self._make_child_func(token.child))
        elif isinstance(token, CommandExpr):
            return self._make_child_func(token)
        else:
            assert False, "unknown token: {}".format(token)

    def _register_commands(self):
        self.register_command(
            ["video", "videos", "vid", "vids"],
            lambda args: self._filter.set_regex_pattern(VIDEO_REGEX, re.IGNORECASE))

        self.register_command(
            ["image", "images", "img", "imgs"],
            lambda args: self._filter.set_regex_pattern(IMAGE_REGEX, re.IGNORECASE))

        self.register_command(
            ["archive", "archives", "arch", "ar"],
            lambda args: self._filter.set_regex_pattern(ARCHIVE_REGEX, re.IGNORECASE))

        self.register_command(
            ["r", "rx", "re", "regex"],
            lambda args: self._filter.set_regex_pattern(args[0], re.IGNORECASE))

        self.register_command(
            "today",
            lambda args: self._filter.set_time(
                datetime.datetime.combine(
                    datetime.date.today(), datetime.datetime.min.time()).timestamp(),
                operator.ge))

        self.register_command(
            "len",
            lambda args: self._filter.set_length(int(args[1]), get_compare_operator(args[0])))

        self.register_command(
            "size",
            lambda args: self._filter.set_size(bytefmt.dehumanize(args[1]),
                                               get_compare_operator(args[0])))

        self.register_command(
            "random",
            lambda args: self._filter.set_random(float(args[0])))

        self.register_command(
            "pick",
            lambda args: self._filter.set_random_pick(int(args[0])))

        self.register_command(
            ["folder", "folders", "dir", "dirs", "directories"],
            lambda args: self._filter.set_folder())

        self.register_command(
            ["G", "Glob"],
            lambda args: self._filter.set_pattern(args[0], case_sensitive=True),
            help="Case-sensitive glob matching")

        self.register_command(
            ["g", "glob"],
            lambda args: self._filter.set_pattern(args[0], case_sensitive=False),
            help="Case-insensitive glob matching")

        self.register_command(
            ["f", "fuz", "fuzz", "fuzzy"],

            lambda args: self._filter.set_fuzzy(args[0]),
            help="Fuzzy match the filename")

        self.register_command(
            ["ascii"],
            lambda args: self._filter.set_ascii(True),
            help="filenames with only ASCII character")

        self.register_command(
            ["nonascii"],
            lambda args: self._filter.set_ascii(False),
            help="filenames with some non-ASCII character")

        self.register_command(
            ["help", "h"],
            lambda args: self.print_help())

    def print_help(self, fout=sys.stdout):
        for aliases, func, help in self._commands.values():
            print("{}".format(", ".join(["/" + x for x in aliases])), file=fout)
            print("  {}".format(help or ""), file=fout)
            print(file=fout)

    def register_command(self,
                         aliases: Union[str, List[str]],
                         func: Callable,
                         help: Optional[str]=None) -> None:
        if isinstance(aliases, list):
            for name in aliases:
                assert name not in self._commands
                self._commands[name] = (aliases, func, help)
        else:
            self._commands[aliases] = ([aliases], func, help)

    def parse(self, pattern):
        if pattern == "":
            self._filter.set_none()
        elif pattern.startswith("/"):
            command, *rest = pattern[1:].split(" ", 1)
            args = shlex.split(rest[0]) if rest else []
            print("Args:", args)
            cmd = self._commands.get(command, None)
            if cmd is None:
                print("Controller.set_filter: unknown command: {}".format(command))
            else:
                _, func, _ = cmd
                func(args)
        else:
            func = self.parse2(pattern)
            self._filter.set_match_func(func)


# EOF #
