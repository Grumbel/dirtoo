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


from typing import Optional, IO

import logging
import sys
import textwrap

from dirtoo.fileview.filter_command_parser import FilterCommandParser
from dirtoo.fileview.filter_expr_parser import FilterExprParser
from dirtoo.fileview.match_func import MatchFunc

logger = logging.getLogger(__name__)


class FilterParser:

    def __init__(self) -> None:
        self._command_parser = FilterCommandParser()
        self._expr_parser = FilterExprParser()

    def print_help(self, fout: IO[str] = sys.stdout) -> None:
        for aliases, doc in self._expr_parser._func_factory.get_docs():
            fout.write("{}:{}".format(aliases[0],
                                      textwrap.dedent(doc or "")))
            if len(aliases) > 1:
                print("Aliases: {}".format(", ".join(aliases[1:])), file=fout)
            print(file=fout)
            print(file=fout)

    def parse(self, pattern: str) -> Optional[MatchFunc]:
        if pattern.startswith("/"):
            self._command_parser.parse(pattern)
            return None
        else:
            return self._expr_parser.parse(pattern)


# EOF #
