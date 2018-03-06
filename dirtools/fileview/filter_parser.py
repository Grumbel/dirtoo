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


import logging
import sys

from dirtools.fileview.filter_command_parser import FilterCommandParser
from dirtools.fileview.filter_expr_parser import FilterExprParser

logger = logging.getLogger(__name__)


class FilterParser:

    def __init__(self, filter):
        self._filter = filter
        self._command_parser = FilterCommandParser(filter)
        self._expr_parser = FilterExprParser()

    def print_help(self, fout=sys.stdout):
        for aliases, func, help in self._commands.values():
            print("{}".format(", ".join(["/" + x for x in aliases])), file=fout)
            print("  {}".format(help or ""), file=fout)
            print(file=fout)

    def parse(self, pattern: str) -> None:
        if pattern == "":
            self._filter.set_none()
        elif pattern.startswith("/"):
            self._command_parser.parse(pattern)
        else:
            func = self._expr_parser.parse(pattern)
            self._filter.set_match_func(func)


# EOF #
