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


from typing import Any, List

import logging

from dirtoo.fileview.match_func import MatchFunc, AndMatchFunc, OrMatchFunc, ExcludeMatchFunc

logger = logging.getLogger(__name__)


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
        return "OR"


class AndKeywordExpr:

    def __init__(self, s: str, loc: int, toks: List[Any]) -> None:
        self.keyword = toks[0].upper()

    def __repr__(self):
        return "AND"


class FilterExprParser:

    def __init__(self) -> None:
        from dirtoo.fileview.match_func_factory import MatchFuncFactory

        self._grammar = self._make_grammar()
        self._func_factory = MatchFuncFactory()

    def _make_grammar(self):
        from pyparsing import (QuotedString, ZeroOrMore, Combine,
                               Literal, Optional, OneOrMore,
                               Regex, CaselessKeyword)

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
        command = Regex(r'[^\s:]+') + Literal(":").suppress() + (quotedstring | word)
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
        result: List = [[]]

        for token in tokens:
            if isinstance(token, AndKeywordExpr):
                pass  # ignore
            elif isinstance(token, OrKeywordExpr):
                result.append([])
            elif isinstance(token, (IncludeExpr, ExcludeExpr, CommandExpr)):
                result[-1].append(token)
            else:
                assert False, "unknown token: {}".format(token)

        # Remove empty lists that result from unterminated OR keywords
        result = list(filter(bool, result))

        return result

    def parse(self, text: str) -> MatchFunc:
        tokens = self._grammar.parseString(text, parseAll=True)
        parsed_tokens = self._parse_tokens(tokens)

        or_funcs: List[MatchFunc] = []
        for tokens in parsed_tokens:
            and_funcs: List[MatchFunc] = []
            for token in tokens:
                and_funcs.append(self._make_func(token))
            and_funcs = sorted(and_funcs, key=lambda x: x.cost())
            or_funcs.append(AndMatchFunc(and_funcs))

        or_funcs = sorted(or_funcs, key=lambda x: x.cost())
        return OrMatchFunc(or_funcs)

    def _make_func(self, token):
        if isinstance(token, IncludeExpr):
            return self._func_factory.make_match_func(token.child)
        elif isinstance(token, ExcludeExpr):
            return ExcludeMatchFunc(self._func_factory.make_match_func(token.child))
        elif isinstance(token, CommandExpr):
            return self._func_factory.make_match_func(token)
        else:
            assert False, "unknown token: {}".format(token)


# EOF #
