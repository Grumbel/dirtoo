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


from typing import Any, List, Tuple, Callable

import logging
import re
import datetime
import operator

import bytefmt

from dirtools.fileview.match_func import (
    MatchFunc,
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
    ContainsMatchFunc,
    DateMatchFunc,
    TimeMatchFunc
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


class IncludeExpr:

    def __init__(self, s: str, loc: int, toks: List[Any]) -> None:
        self.child = toks[0]

    def __repr__(self):
        return f"Include({self.child})"


class ExcludeExpr:

    def __init__(self, s: str, loc: int, toks: List[Any]) -> None:
        self.child = toks[0]

    def __repr__(self):
        return f"Exclude({self.child})"


class CommandExpr:

    def __init__(self, s: str, loc: int, toks: List[Any]) -> None:
        self.command = toks[0]
        self.arg = toks[1]

    def __repr__(self):
        return f"Command({self.command}:{self.arg})"


class OrKeywordExpr:

    def __init__(self, s: str, loc: int, toks: List[Any]) -> None:
        self.keyword = toks[0].upper()

    def __repr__(self):
        return f"OR"


class AndKeywordExpr:

    def __init__(self, s: str, loc: int, toks: List[Any]) -> None:
        self.keyword = toks[0].upper()

    def __repr__(self):
        return f"AND"


class FilterExprParser:

    def __init__(self):
        self._grammar = self._make_grammar()

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
        exclude = (Literal("-") | Literal("^")).suppress() + (quotedstring | command | word)
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

    def parse(self, text: str) -> MatchFunc:
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
            if child.command in ["g", "glob"]:
                return GlobMatchFunc(child.arg, case_sensitive=False)
            elif child.command in ["G", "Glob"]:
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
            elif child.command in ["f", "fuz", "fuzz", "fuzzy"]:
                return FuzzyMatchFunc(child.arg)
            elif child.command == "date":
                return DateMatchFunc(child.arg)
            elif child.command == "time":
                return TimeMatchFunc(child.arg)
            elif child.command == "charset" or child.command == "encoding":
                if child.arg == "ascii":
                    return AsciiMatchFunc()
                else:
                    logger.error("unknown charset in command: %s", child)
                    return FalseMatchFunc()
            elif child.command == "contains":
                return ContainsMatchFunc(child.arg, case_sensitive=False)
            elif child.command == "Contains":
                return ContainsMatchFunc(child.arg, case_sensitive=True)
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


# EOF #
