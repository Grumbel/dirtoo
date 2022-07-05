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


import unittest
from typing import Any, Optional

import pyparsing

from dirtoo.expr import Parser, Context, Expr


class ExprTestCase(unittest.TestCase):

    def test_expr(self) -> None:
        expressions = [
            "5+5*7",
            "True and not False",
            "5.0 + 10.0",
            "30 / 5 * 6",
            "   30 * 5 / 6 + 1   ",
            "3 + 4 * 5 * 6",
            "3 - 4 / 5 * 6",
            "1 + 2 * 3 - 4 / 5 * 6",
            "1+2*3-4/5*6/7-8",
            "abs(-5) * abs(-9)",
            "True == (not False)",
            "5.0 / 2",
            "5 // 2",
            "float('5.234')",
            "int(5.234)",
        ]

        parser = Parser()
        ctx = Context()

        for text in expressions:
            tree: Optional[Expr] = None
            result: Any
            try:
                tree = parser.parse(text)
                result = tree.eval(ctx)
            except pyparsing.ParseException as err:
                result = err

            try:
                expected = eval(text)  # pylint: disable=W0123
            except SyntaxError as err:
                expected = err

            self.assertEqual(result, expected, "for expression: '{}'\n\n{}".format(text, tree))


# EOF #
