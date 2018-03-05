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


import unittest

from dirtools.fileview.filter_parser import FilterParser


class UtilTestCase(unittest.TestCase):

    def test_filter_parser(self):
        grammar = FilterParser._make_grammar(None)
        grammar.leaveWhitespace()

        test_strings = [
            r'',
            r'  "fooba\"r""join" "no" "join" "do""join"  "bar"   -barfoo size:>50  more ',
            r' more "-less:oeu" -size:>50  glob:"*.png""more"      ',
            r'more  -moroe size:4 AND foo',
            r' -"more AND oeu" OR -foo bar AND bar -site:foo ',
            r'-notthis more OR not AND bla glob:bla',
            r' unterminated AND ',
            r' -unterminated OR ',
            r' AND unterminated ',
            r' OR unterminated ',
        ]

        print()
        parser = FilterParser(None)
        for string in test_strings:
            # print(grammar.parseString(string, parseAll=True))
            parser.parse2(string)

# EOF #
