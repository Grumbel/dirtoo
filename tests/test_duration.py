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

from dirtools.duration import dehumanize, dehumanize_hms, dehumanize_dot, dehumanize_unit


class DurationTestCase(unittest.TestCase):

    def setUp(self):
        self.hms_tests = [
            ("12s", 12),
            ("1m12s", 72),
            ("5h1m12s", 18072),
            ("5h12s", 18012),
            ("5h1m", 18060),
        ]

        self.dot_tests = [
            ("12:00", 720),
            ("12:11h", 43860),
            ("12:11:13", 43873),
            ("1:11m", 71),
        ]

        self.unit_tests = [
            ("12s", 12),
            ("13.23h", 47628),
            ("14m", 840),
            (".5h", 1800),
            ("0.5h", 1800),
            ("5.h", 18000),
            ("5.0h", 18000),
            ("234", 234),
        ]

        self.fail_tests = [
            "234z",
            "z342",
            "12:11:10:10",
            "12:11:10.110",
            "12h:11",
            "12h11",
            " 12:11 ",
            " 12:11",
            "12:11 "
        ]

    def test_dehumanize_hms(self):
        for text, expected in self.hms_tests:
            self.assertEqual(dehumanize_hms(text), expected)

    def test_dehumanize_dot(self):
        for text, expected in self.dot_tests:
            self.assertEqual(dehumanize_dot(text), expected)

    def test_dehumanize_unit(self):
        for text, expected in self.unit_tests:
            self.assertEqual(dehumanize_unit(text), expected)

    def test_dehumanize(self):
        for text, expected in self.unit_tests + self.dot_tests + self.hms_tests:
            self.assertEqual(dehumanize(text), expected)

    def test_dehumanize_fail(self):
        for text in self.fail_tests:
            self.assertEqual(dehumanize(text), None)


# EOF #
