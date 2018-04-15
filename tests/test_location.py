# dirtool.py - diff tool for directories
# Copyright (C) 2017 Ingo Ruhnke <grumbel@gmail.com>
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
import os

from dirtools.fileview.location import Location, Payload


class LocationTestCase(unittest.TestCase):

    def test_from_human(self):
        humans = [
            ("/",
             "file:///"),

            ("/tmp/file.rar://rar",
             "file:///tmp/file.rar://rar"),

            ("/tmp/file",
             "file:///tmp/file"),

            ("file:///tmp/file2",
             "file:///tmp/file2"),

            ("file:///tmp/file.rar//rar:foo.bar",
             "file:///tmp/file.rar//rar:foo.bar")
        ]

        for human, expected in humans:
            location = Location.from_human(human)
            self.assertEqual(location.as_url(), expected)

    def test_from_human_path(self):
        result = Location.from_human(".")
        expected = Location.from_path(os.getcwd())
        self.assertEqual(result, expected)

        result = Location.from_human("")
        expected = Location.from_path("/")
        self.assertEqual(result, expected)

    def test_location_init(self):
        ok_texts = [
            ("file:///home/juser/test.rar//rar:file_inside.rar",
             ("file", "/home/juser/test.rar", [Payload("rar", "file_inside.rar")])),

            ("file:///home/juser/test.rar",
             ("file", "/home/juser/test.rar", [])),

            ("file:///home/juser/test.rar//rar",
             ("file", "/home/juser/test.rar", [Payload("rar", "")])),

            ("file:///tmp/",
             ("file", "/tmp", [])),

            ("file:///home/juser/test.rar//rar:file_inside.rar//rar:file.txt",
             ("file", "/home/juser/test.rar", [Payload("rar", "file_inside.rar"), Payload("rar", "file.txt")])),

            ("file:///test.rar//rar:one//rar:two//rar:three",
             ("file", "/test.rar", [Payload("rar", "one"), Payload("rar", "two"), Payload("rar", "three")]))
        ]

        for text, (protocol, abspath, payloads) in ok_texts:
            location = Location.from_url(text)
            self.assertEqual(location._protocol, protocol)
            self.assertEqual(location._path, abspath)
            self.assertEqual(location._payloads, payloads)

        fail_texts = [
            "/home/juser/test.rar",
            "file://test.rar",
            "file:///test.rar//rar:oeu//"
            "file:///test.rar///rar:foo"
        ]

        for text in fail_texts:
            with self.assertRaises(Exception) as context:
                Location.from_string(text)

    def test_location_parent(self):
        parent_texts = [
            ("file:///home/juser/test.rar//rar:file_inside.rar",
             ("file", "/home/juser/test.rar", [Payload("rar", "")])),

            ("file:///home/juser/test.rar",
             ("file", "/home/juser", [])),

            ("file:///home/juser/test.rar//rar",
             ("file", "/home/juser", [])),

            ("file:///tmp/",
             ("file", "/", [])),

            ("file:///home/juser/test.rar//rar:file_inside.rar//rar:file.txt",
             ("file", "/home/juser/test.rar", [Payload("rar", "file_inside.rar"), Payload("rar", "")])),

            ("file:///home/juser/test.rar//rar:file_inside.rar//rar",
             ("file", "/home/juser/test.rar", [Payload("rar", "")])),

            ("file:///test.rar//rar:one//rar:two//rar:three",
             ("file", "/test.rar", [Payload("rar", "one"), Payload("rar", "two"), Payload("rar", "")]))
        ]

        for text, (protocol, abspath, payloads) in parent_texts:
            location = Location.from_url(text)
            location = location.parent()
            self.assertEqual(location._protocol, protocol, text)
            self.assertEqual(location._path, abspath, text)
            self.assertEqual(location._payloads, payloads, text)

    def test_location_join(self):
        join_texts = [
            ("file:///home/juser/test.rar//rar",
             "foobar",
             ("file", "/home/juser/test.rar", [Payload("rar", "foobar")])),

            ("file:///home/juser/test.rar//rar:foo.rar//rar",
             "foobar.png",
             ("file", "/home/juser/test.rar", [Payload("rar", "foo.rar"), Payload("rar", "foobar.png")])),

            ("file:///home/juser/test.rar//rar:foo.rar//rar:foobar",
             "bar.png",
             ("file", "/home/juser/test.rar", [Payload("rar", "foo.rar"), Payload("rar", "foobar/bar.png")])),
        ]

        for base_text, join_text, (protocol, abspath, payloads) in join_texts:
            base = Location.from_url(base_text)
            result = Location.join(base, join_text)

            self.assertEqual(result._protocol, protocol, base_text)
            self.assertEqual(result._path, abspath, base_text)
            self.assertEqual(result._payloads, payloads, base_text)

    def test_ancestry(self):
        location = Location.from_url("file:///home/juser/test.rar//rar:bar/foo.zip//zip:bar.png")
        result = location.ancestry()
        expected = [
            Location.from_url('file:///'),
            Location.from_url('file:///home'),
            Location.from_url('file:///home/juser'),
            Location.from_url('file:///home/juser/test.rar//rar'),
            Location.from_url('file:///home/juser/test.rar//rar:bar'),
            Location.from_url('file:///home/juser/test.rar//rar:bar/foo.zip//zip'),
            Location.from_url('file:///home/juser/test.rar//rar:bar/foo.zip//zip:bar.png')
        ]
        self.assertEqual(result, expected)

    def test_basename(self):
        testcases = [
            ("file:///home/juser/test.rar//rar:bar/foo.zip//zip:bar.png",
             "bar.png"),

            ("file:///home/juser/test.rar//rar:bar/foo.zip//zip",
             "foo.zip//zip"),

            ("file:///home/juser/foo.zip//zip:test.rar",
             "test.rar"),

            ("file:///home/juser/test.rar",
             "test.rar"),
        ]

        for url, expected in testcases:
            location = Location.from_url(url)
            result = location.basename()
            self.assertEqual(result, expected)


# EOF #
