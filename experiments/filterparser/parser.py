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


import sys

from pyparsing import (QuotedString, ZeroOrMore, Word, CharsNotIn, Suppress, Literal, Combine, OneOrMore,
                       Forward, Optional, White, LineEnd, StringStart, StringEnd, Regex)


def main(argv):

    escape = Suppress(Literal("\\")) + Word(" '\"", exact=1)
    word = Combine(OneOrMore(escape | Regex(r'[^\s\\]+')))
    command = ZeroOrMore(QuotedString('"', escChar='\\') | QuotedString("'", escChar='\\') | word)

    for arg in argv[1:]:
        result = command.parseString(arg, parseAll=True)
        print(arg)
        print(result)
        print()


if __name__ == "__main__":
    main(sys.argv)


# EOF #
