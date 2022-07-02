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

from dirtoo.filter.filter_expr_parser import FilterExprParser


class UtilTestCase(unittest.TestCase):

    def test_filter_parser(self) -> None:
        parser = FilterExprParser()
        grammar = parser._grammar
        # grammar.leaveWhitespace()

        test_strings = [
            (r'',
             '[]'),

            (r'  "fooba\"r""join" "no" "join" "do""join"  "bar"   -barfoo size:>50  more ',
             '[Include(fooba"rjoin), Include(no), Include(join), Include(dojoin), Include(bar), '
             'Exclude(barfoo), Include(Command(size:>50)), Include(more)]'),

            (r' more "-less:oeu" -size:>50  glob:"*.png""more"      ',
             '[Include(more), Include(-less:oeu), Exclude(Command(size:>50)), Include(Command(glob:*.pngmore))]'),

            (r'more  -moroe size:4 AND foo',
             '[Include(more), Exclude(moroe), Include(Command(size:4)), AND, Include(foo)]'),

            (r' -"more AND oeu" OR -foo bar AND bar -site:foo ',
             '[Exclude(more AND oeu), OR, Exclude(foo), Include(bar), AND, Include(bar), Exclude(Command(site:foo))]'),

            (r'-notthis more OR not AND bla glob:bla',
             '[Exclude(notthis), Include(more), OR, Include(not), AND, Include(bla), Include(Command(glob:bla))]'),

            (r' unterminated AND ',
             '[Include(unterminated), AND]'),

            (r' text command:arg ',
             '[Include(text), Include(Command(command:arg))]'),

            (r' -unterminated OR ',
             '[Exclude(unterminated), OR]'),

            (r' AND unterminated ',
             '[AND, Include(unterminated)]'),

            (r' OR unterminated ',
             '[OR, Include(unterminated)]')
        ]

        parser = FilterExprParser()
        for text, expected in test_strings:
            parse_results = grammar.parseString(text, parseAll=True)
            result = str(parse_results)
            self.assertEqual(result, expected)
            parser.parse(text)

# EOF #
