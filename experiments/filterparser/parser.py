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

from pyparsing import (QuotedString, ZeroOrMore, Combine, OneOrMore, Regex, ParserElement)


def escape_handler(s: str, loc: int, toks: List[str]) -> str:
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


def make_bnf() -> ParserElement:
    escape = Combine(Regex(r'\\.')).setParseAction(escape_handler)
    word = Combine(OneOrMore(escape | Regex(r'[^\s\\]+')))
    command = ZeroOrMore(QuotedString('"', escChar='\\') | QuotedString("'", escChar='\\') | word)
    return command


def main(argv: List[str]) -> None:
    grammar = make_bnf()

    for arg in argv[1:]:
        results = grammar.parseString(arg, parseAll=True)
        print("Input:", arg)
        for result in results:
            print(result)
        print()


if __name__ == "__main__":
    main(sys.argv)


# EOF #
